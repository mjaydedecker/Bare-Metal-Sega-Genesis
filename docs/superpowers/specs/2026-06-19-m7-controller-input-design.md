# M7 — Controller Input: Design

**Date:** 2026-06-19
**Milestone:** M7 (Controller Input) — last of the Phase-1 core loop.
**Status:** design approved; implementation plan to follow.
**Target hardware:** Raspberry Pi 2 (`RASPPI=2`, AArch32, `kernel7.img`).

## Goal

Let a USB gamepad control the emulated Genesis: wire libretro's input callbacks
to Circle's USB gamepad device so the D-pad and buttons work in a game.

**Done when:** with a USB gamepad connected at boot, all four D-pad directions,
the six face buttons (A/B/C/X/Y/Z), Start, and Mode register in a game on
hardware; with no pad connected, the game still boots and runs (logs "No USB
gamepad found").

## Decisions (and rejected alternatives)

| Decision | Choice | Rejected / deferred |
|----------|--------|---------------------|
| Controller support | **Generic `CUSBGamePadDevice`** (normalized, any USB HID pad) | Targeting one specific controller |
| Genesis pad type | **6-button** (`RETRO_DEVICE_MDPAD_6B`) | 3-button; runtime toggle |
| State access | **Poll `GetReport()` per frame** | Event/status-handler model (concurrency) |
| Players | **1 (port 0)** | 2-player (port 1) |
| Mapping | **Fixed, role-based default** | Runtime remap / config-file mapping |

Circle's gamepad layer normalizes every supported controller into a
`TGamePadState.buttons` bitmask of role bits (`GamePadButtonA/B/X/Y/LB/RB/Start/
Select`, and `Up/Down/Left/Right` with hat/analog already folded in), so the
mapping is controller-agnostic.

## Architecture

```
CKernel ──owns──> Gamepad ──wraps──> CUSBGamePadDevice ("upad1")
   │
   └──sets──> g_gamepad ──used by──> input_poll_cb / input_state_cb
```

- **`Gamepad`** (`src/input/gamepad.{h,cpp}`) — finds and reads the USB gamepad;
  hides Circle, knows nothing about libretro.
- **`joypad_state`** (`src/input/joypad_map.{h,cpp}`) — pure function mapping a
  button bitmask + libretro id → pressed/released; host-testable.
- **input callbacks** (`src/libretro/callbacks.cpp`) — `input_poll_cb` snapshots
  the pad; `input_state_cb` calls `joypad_state`. Reached via `g_gamepad`.
- **`CKernel`** — owns `Gamepad m_Gamepad`, initializes it after USB, sets
  `g_gamepad`, and sets port 0 to a 6-button MD pad.

Dependency direction: `kernel → gamepad`, `kernel → callbacks → (g_gamepad) →
gamepad`, `callbacks → joypad_map`. `Gamepad` depends only on Circle USB;
`joypad_map` depends on nothing (host-testable).

## Components

### `Gamepad` (`src/input/gamepad.h` / `.cpp`)

```cpp
class Gamepad
{
public:
    Gamepad(CDeviceNameService *pNameService);

    boolean  Initialize();              // find "upad1"; FALSE if no pad
    boolean  IsPresent() const;         // a device was found
    void     Poll();                    // snapshot GetReport()->buttons
    unsigned Buttons() const;           // cached TGamePadButton bitmask (0 if none)

private:
    CDeviceNameService *m_pNameService;
    CUSBGamePadDevice  *m_pDevice;       // 0 if not found
    unsigned            m_Buttons;
};
```
- `Initialize()`: `m_pDevice = (CUSBGamePadDevice *)
  m_pNameService->GetDevice("upad", 1, FALSE);` return `m_pDevice != 0`.
- `Poll()`: `if (m_pDevice) { const TGamePadState *s = m_pDevice->GetReport();
  m_Buttons = s ? s->buttons : 0; } else m_Buttons = 0;`
- `Buttons()`: returns `m_Buttons`.

### `joypad_map` (`src/input/joypad_map.h` / `.cpp`) — pure, host-testable

```cpp
// retro_id is a RETRO_DEVICE_ID_JOYPAD_* value; buttons is a TGamePadButton
// bitmask. Returns 1 if the mapped button is pressed, else 0.
int16_t joypad_state(unsigned buttons, unsigned retro_id);
```
The header defines the button-bit constants used (mirroring `TGamePadButton`).
`gamepad.cpp`/`callbacks.cpp` add a `static_assert` that these equal Circle's
`GamePadButton*` values so they cannot drift. Mapping:

| `retro_id` | bit |
|---|---|
| `RETRO_DEVICE_ID_JOYPAD_UP/DOWN/LEFT/RIGHT` | `Up/Down/Left/Right` |
| `RETRO_DEVICE_ID_JOYPAD_A` | `A` |
| `RETRO_DEVICE_ID_JOYPAD_B` | `B` |
| `RETRO_DEVICE_ID_JOYPAD_X` | `X` |
| `RETRO_DEVICE_ID_JOYPAD_Y` | `Y` |
| `RETRO_DEVICE_ID_JOYPAD_L` | `LB` |
| `RETRO_DEVICE_ID_JOYPAD_R` | `RB` |
| `RETRO_DEVICE_ID_JOYPAD_START` | `Start` |
| `RETRO_DEVICE_ID_JOYPAD_SELECT` | `Select` |
| anything else | (returns 0) |

We feed physical buttons to libretro ids by role; the core maps libretro ids →
the six Genesis buttons (its standard `MDPAD_6B` mapping). Exact A/B/C feel is
the core's default; remapping is deferred.

### `callbacks.cpp`

```cpp
Gamepad *g_gamepad = 0;

void input_poll_cb(void)
{
    if (g_gamepad) g_gamepad->Poll();
}

int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)index;
    if (port != 0 || device != RETRO_DEVICE_JOYPAD || g_gamepad == 0)
        return 0;
    return joypad_state(g_gamepad->Buttons(), id);
}
```
`g_gamepad` is forward-declared in `callbacks.h` (`class Gamepad; extern Gamepad
*g_gamepad;`).

### `CKernel` (`src/kernel.{h,cpp}`)

- Add `Gamepad m_Gamepad;` member (constructed `m_Gamepad(&m_DeviceNameService)`).
- In `Initialize()`, after USB init: `m_Gamepad.Initialize()`; log notice if no
  pad found (non-fatal).
- In `Run()`, after `retro_load_game`:
  ```cpp
  #define RETRO_DEVICE_MDPAD_6B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
  retro_set_controller_port_device(0, RETRO_DEVICE_MDPAD_6B);
  retro_set_controller_port_device(1, RETRO_DEVICE_NONE);
  g_gamepad = &m_Gamepad;
  ```

## Data flow — per frame

1. `retro_run()` calls `input_poll_cb` once → `Gamepad::Poll()` snapshots the
   current button bitmask.
2. `retro_run()` calls `input_state_cb` repeatedly; for port 0 / joypad each
   queried `id` → `joypad_state(buttons, id)` → 0/1.
3. The core maps libretro ids → Genesis buttons per its 6-button pad.

## Error handling

- **No pad at boot:** `Initialize()` returns FALSE; `Poll()`/`Buttons()` yield 0;
  `input_state_cb` returns 0. Game runs; log `LogNotice` "No USB gamepad found."
- **`g_gamepad` null:** callbacks return 0 (safe).
- **`GetReport()` null (transient):** cache 0 that frame (no stuck buttons).
- **Non-port-0 / non-joypad / other index:** return 0.
- **Hotplug removal:** out of scope — assume connected at boot and kept. Circle
  PnP may invalidate the pointer on removal; PnP-safe re-acquire is deferred.
- **Generic pad reporting the D-pad only as a raw hat/axis** (not folded into the
  normalized direction bits): directions won't register; a hat/axis fallback is a
  deferred follow-up (known limitation, not a mystery).
- No allocation in the hot path.

## Testing

### Host unit test (the mapping is the bug-prone part)
Extend the `test/` harness with tests for `joypad_state`:
- each `RETRO_DEVICE_ID_JOYPAD_*` returns 1 only when its mapped bit is set;
- multiple buttons set at once resolve independently;
- unmapped ids and non-zero ports → 0; empty bitmask → all 0.

### Regression + build gates
Existing `blit` host tests still pass; clean cross-compile of `kernel7.img`.

### On-hardware acceptance
- Pad connected at boot: all four D-pad directions, A/B/C/X/Y/Z, Start, Mode
  register and act sensibly in a game.
- No pad: game still boots/runs; serial shows "No USB gamepad found."

## Files

- `src/input/gamepad.h` / `.cpp` — new (`Gamepad`).
- `src/input/joypad_map.h` / `.cpp` — new (pure `joypad_state`).
- `src/libretro/callbacks.cpp` / `.h` — implement input callbacks, add `g_gamepad`.
- `src/kernel.h` / `kernel.cpp` — `m_Gamepad` member, init, 6-button setup.
- `Makefile` — add `src/input/*.o` to `OBJS`/`EXTRACLEAN`; add `joypad_map` to the host `test/` build.
- `test/` — host tests for `joypad_state`.

## Deferred work
Second player (port 1), runtime/config button remapping, 3-button toggle,
USB hotplug (PnP-safe re-acquire), and raw hat/axis D-pad fallback for
non-normalized pads.
