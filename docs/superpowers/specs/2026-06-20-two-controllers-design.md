# Two-Controller Support — Design

**Date:** 2026-06-20
**Status:** approved design, pending implementation plan
**Related:** FSD §4.6 (Controller Input), M7 input status (single-pad input working on hardware)

## Summary

Add Player 2 support: two USB gamepads drive the Genesis's two native controller
ports independently, and either controller can navigate the menus and open the
pause menu. This evolves the current single-pad input layer into a two-player
`Gamepads` manager.

Scope is **two players** (the Genesis's native port count and the FSD target).
Multitap/4-player and per-pad remapping are out of scope (see Out of Scope).

## Background / Constraint

Current input (single player, hardware-verified in M7):
- `gamepad.cpp` holds **one file-static `s_buttons`**; the status handler ignores
  `nDeviceIndex`.
- `Gamepad` acquires only `"upad1"`.
- `input_state_cb` hard-returns 0 for any port other than 0.
- The kernel sets `retro_set_controller_port_device(1, RETRO_DEVICE_NONE)`.

**Hard constraint:** Circle's `CUSBGamePadDevice::RegisterStatusHandler` takes a
bare function pointer with **no userdata**, so a handler cannot be bound to an
instance. Button state must live in file-static storage the handler can reach.
The design therefore uses one **dedicated handler per device slot** writing to a
per-slot static cache — avoiding any assumption about how Circle numbers
`nDeviceIndex` (an undocumented detail that caused trouble in M7).

## Architecture

### `src/input/gamepad.{h,cpp}` — evolve `Gamepad` into `Gamepads`

A single class manages both pads.

- `static const unsigned MAX_PADS = 2;`
- `static volatile unsigned s_buttons[MAX_PADS];` — updated asynchronously by the
  USB report handlers (one aligned word per slot, read/written atomically).
- `static unsigned decode_buttons(const TGamePadState *pState);` — the existing
  raw-button + hat + analog-axis folding logic, factored out so both pads share
  it (DRY).
- Two thin handlers:
  - `static void Handler0(unsigned, const TGamePadState *p) { s_buttons[0] = decode_buttons(p); }`
  - `static void Handler1(unsigned, const TGamePadState *p) { s_buttons[1] = decode_buttons(p); }`
- `CUSBGamePadDevice *m_pDevice[MAX_PADS];`

Public API:

| Method | Behavior |
|---|---|
| `void Poll()` | For each empty slot `i`, `GetDevice("upad", i+1, FALSE)`; on acquire, register slot `i`'s handler. Lazy plug-and-play (same model as today). |
| `unsigned Buttons(unsigned port)` | `s_buttons[port]`, or 0 if `port >= MAX_PADS`. |
| `bool Present(unsigned port)` | `port < MAX_PADS && m_pDevice[port] != 0`. |
| `unsigned MenuButtons()` | `s_buttons[0] | s_buttons[1]` — the "either controller" source for menus/hotkey. |
| `unsigned Count()` | Number of present pads. |

The `static_assert`s tying `GP_*` direction bits to Circle's `GamePadButton*`
constants are retained.

### Data flow

- **Gameplay:** `input_state_cb(port, device, index, id)` accepts ports 0 **and**
  1 (`port < 2`), returning `joypad_state(g_gamepad->Buttons(port), id)`. The
  pure `joypad_state` mapper is unchanged and reused per port. The kernel sets
  `retro_set_controller_port_device(1, RETRO_DEVICE_MDPAD_6B)` (was `NONE`).
- **Menus + pause hotkey:** `RomMenu`, `PauseMenu`, `SettingsScreen` take a
  `Gamepads*` and read `MenuButtons()` for navigation (replacing `Buttons()`);
  `Poll()` calls are unchanged. The kernel's Start+Select hotkey check uses
  `MenuButtons()`.
- The `g_gamepad` global (`callbacks.cpp`) becomes `Gamepads*`; `input_poll_cb`
  still calls `g_gamepad->Poll()`.

### Pad → port assignment

First-enumerated USB pad = Player 1 (`upad1`), second = Player 2 (`upad2`), per
Circle's enumeration order. Not user-remappable (remap is deferred).

## Error Handling / Edge Cases

- **One pad only:** P1 works; `Buttons(1)` returns 0. Port 2 is advertised to the
  core but idle — single-player games are unaffected.
- **No pad:** both slots read 0 (unchanged from today).
- **Hotplug removal:** out of scope (consistent with the M7 deferral). A removed
  pad's last bitmask may persist until reboot. Documented as a known limitation.
- **Out-of-range port:** `Buttons()`/`Present()` return 0/false defensively.

## Testing

**Host unit tests** (`test/`):
- Existing `test_joypad` already covers `joypad_state` per-bitmask mapping — the
  same function serves both ports, so per-port behavior is covered.
- Add an assertion that menu-combining two pad bitmasks is a bitwise OR
  (e.g. `(GP_UP) | (GP_START)` yields both bits), documenting the `MenuButtons`
  contract.

(The `Gamepads` manager itself is Circle-dependent and verified by the cross
build + on hardware, consistent with how `Gamepad` is handled today.)

**Hardware verification:**
- Two pads control Player 1 and Player 2 independently in a 2-player game
  (e.g. Sonic 2 two-player, Streets of Rage).
- Either pad navigates the ROM browser / pause menu / settings, and either opens
  the pause menu via Start+Select.
- With only one pad connected, it plays as Player 1 as before.

## Out of Scope (future specs)

- Multitap / 4+ players (different libretro controller device + per-port
  plumbing).
- Per-pad button remapping / controller-agnostic auto-mapping (FSD
  `controller_1_map` / `controller_2_map`).
- Hotplug removal handling and pad→port reassignment UI.
- 3-button/6-button mode toggle.
