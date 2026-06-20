# Two-Controller Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Support two USB gamepads driving the Genesis's two controller ports independently, with either pad able to navigate menus and open the pause menu.

**Architecture:** Extend the existing `Gamepad` class into a two-pad manager: a per-port static button cache (`s_buttons[2]`) updated by one dedicated USB report handler per device, plus `Buttons(port)` for gameplay and `MenuButtons()` (port0|port1) for menus. `input_state_cb` serves ports 0 and 1 through the unchanged pure `joypad_state` mapper; the kernel enables port 1 as `MDPAD_6B`.

**Tech Stack:** C++ (bare-metal, Circle framework), Genesis-Plus-GX-Wide libretro core, host-side g++ unit tests.

## Naming note (deviation from spec)

The spec names the class `Gamepads`. This plan keeps the existing name **`Gamepad`** and extends it in place. Rationale: renaming the type touches ~8 files (callbacks, kernel, three menus) and cannot be split without a broken intermediate build, whereas extending in place keeps every task compiling green. Behavior matches the spec exactly. If a rename is preferred, it can be a separate mechanical follow-up.

## Global Constraints

- The pure `joypad_state` mapper (`src/input/joypad_map.{h,cpp}`) is unchanged and reused for both ports.
- Circle's `RegisterStatusHandler` takes a bare function pointer (no userdata): each pad slot uses its **own** dedicated handler writing to its own cache slot — no reliance on `nDeviceIndex` numbering.
- `MAX_PADS = 2` (Player 1 + Player 2). Multitap, remapping, and hotplug-removal are out of scope.
- First-enumerated pad (`upad1`) = Player 1, `upad2` = Player 2.
- Build with `-Wall -Wextra`; constructor member-init order matches declaration order (`-Wreorder`).
- Follow existing style: 4-space indent, `m_`-prefixed members.

---

## File Structure

**Modified files only (no new files):**
- `src/input/gamepad.h` / `.cpp` — two-pad cache, dedicated handlers, `Buttons(port)` / `MenuButtons()`.
- `test/test_joypad.cpp` — assertion documenting the combined-pad menu contract.
- `src/libretro/callbacks.cpp` — `input_state_cb` serves ports 0 and 1.
- `src/kernel.cpp` — enable port 1 (`MDPAD_6B`); menus/hotkey use `MenuButtons()`.
- `src/menu/rom_menu.cpp`, `src/menu/pause_menu.cpp`, `src/menu/settings_screen.cpp` — read `MenuButtons()` for navigation.

`gamepad.cpp` is already in the `Makefile` `OBJS`; no Makefile change.

---

## Task 1: Two-pad `Gamepad` (per-port cache + dedicated handlers)

**Files:**
- Modify: `src/input/gamepad.h`
- Modify: `src/input/gamepad.cpp`
- Modify: `test/test_joypad.cpp`

**Interfaces:**
- Consumes: `joypad_state` / `GP_*` (`src/input/joypad_map.h`), unchanged.
- Produces (on `Gamepad`):
  - `static const unsigned MAX_PADS = 2;`
  - `void Poll();`
  - `unsigned Buttons(unsigned port = 0) const;` — `s_buttons[port]`, 0 if `port >= MAX_PADS`. The default keeps existing call sites valid.
  - `unsigned MenuButtons() const;` — `s_buttons[0] | s_buttons[1]`.
  - `boolean IsPresent(unsigned port = 0) const;`

- [ ] **Step 1: Add the combined-pad assertion to the joypad test**

In `test/test_joypad.cpp`, add immediately before the
`printf("All joypad tests passed\n");` line:

```cpp
    // Two pads combine for menu navigation as a bitwise OR; the mapper then
    // reports every pressed button from either pad.
    unsigned combined = (GP_UP) | (GP_START);   // pad0 Up, pad1 Start
    assert(joypad_state(combined, RETRO_DEVICE_ID_JOYPAD_UP));
    assert(joypad_state(combined, RETRO_DEVICE_ID_JOYPAD_START));
```

- [ ] **Step 2: Run the joypad test to confirm it still passes**

Run: `cd test && make test_joypad && ./test_joypad`
Expected: `All joypad tests passed` (the new asserts hold — `joypad_state` is
unchanged; this documents the combine contract).

- [ ] **Step 3: Rewrite the header for two pads**

Replace the body of `src/input/gamepad.h` (the `class Gamepad { ... };` block)
with:

```cpp
class Gamepad
{
public:
    static const unsigned MAX_PADS = 2;   // Player 1 + Player 2

    Gamepad(CDeviceNameService *pNameService);

    // Acquire upad1/upad2 and register each pad's report handler (lazy
    // plug-and-play; pads enumerate a moment after boot).
    void Poll(void);

    // Latest button bitmask for a port (0 if the port has no pad / is invalid).
    unsigned Buttons(unsigned port = 0) const;

    // Combined bitmask of both pads — used for menu navigation / pause hotkey
    // so either controller can drive the UI.
    unsigned MenuButtons(void) const;

    boolean IsPresent(unsigned port = 0) const;

private:
    CDeviceNameService *m_pNameService;
    CUSBGamePadDevice  *m_pDevice[MAX_PADS];
};
```

- [ ] **Step 4: Rewrite the implementation for two pads**

Replace the contents of `src/input/gamepad.cpp` below the `#include` lines
(i.e. everything from the `static volatile unsigned s_buttons` line to the end of
the file) with:

```cpp
// Per-port button caches, updated asynchronously by the USB report handlers.
// One aligned word each, read/written atomically. Static because Circle's
// RegisterStatusHandler takes a bare function pointer (no userdata).
static volatile unsigned s_buttons[Gamepad::MAX_PADS] = { 0, 0 };

// Fold a raw generic-HID gamepad report into our GP_* bitmask: raw digital
// buttons + 8-way hat + analog-axis D-pad. Shared by both pads' handlers.
static unsigned decode_buttons(const TGamePadState *pState)
{
    unsigned b = (unsigned) pState->buttons;   // raw HID digital buttons

    // D-pad as an 8-way hat (0..7) -> direction bits.
    int hat = pState->nhats > 0 ? pState->hats[0] : -1;
    switch (hat)
    {
    case 0: b |= GP_UP;              break;
    case 1: b |= GP_UP | GP_RIGHT;   break;
    case 2: b |= GP_RIGHT;           break;
    case 3: b |= GP_DOWN | GP_RIGHT; break;
    case 4: b |= GP_DOWN;            break;
    case 5: b |= GP_DOWN | GP_LEFT;  break;
    case 6: b |= GP_LEFT;            break;
    case 7: b |= GP_UP | GP_LEFT;    break;
    default: break;                  // centered (or no hat)
    }

    // D-pad as analog axes (X=axis0, Y=axis1): threshold about the midpoint
    // with a 25% deadzone.
    if (pState->naxes >= 2)
    {
        int xmin = pState->axes[0].minimum, xmax = pState->axes[0].maximum;
        int ymin = pState->axes[1].minimum, ymax = pState->axes[1].maximum;
        if (xmax > xmin)
        {
            int mid = (xmin + xmax) / 2, dz = (xmax - xmin) / 4;
            int v = pState->axes[0].value;
            if      (v < mid - dz) b |= GP_LEFT;
            else if (v > mid + dz) b |= GP_RIGHT;
        }
        if (ymax > ymin)
        {
            int mid = (ymin + ymax) / 2, dz = (ymax - ymin) / 4;
            int v = pState->axes[1].value;
            if      (v < mid - dz) b |= GP_UP;
            else if (v > mid + dz) b |= GP_DOWN;
        }
    }

    return b;
}

// One dedicated handler per pad slot (avoids relying on nDeviceIndex numbering).
static void Handler0(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[0] = decode_buttons(pState);
}

static void Handler1(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[1] = decode_buttons(pState);
}

Gamepad::Gamepad(CDeviceNameService *pNameService)
:   m_pNameService(pNameService)
{
    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        m_pDevice[i] = 0;
    }
}

boolean Gamepad::IsPresent(unsigned port) const
{
    return port < MAX_PADS && m_pDevice[port] != 0;
}

void Gamepad::Poll(void)
{
    static TGamePadStatusHandler *const handlers[MAX_PADS] = { Handler0, Handler1 };

    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        if (m_pDevice[i] == 0)   // plug-and-play: pads appear shortly after boot
        {
            m_pDevice[i] = (CUSBGamePadDevice *)
                m_pNameService->GetDevice("upad", i + 1, FALSE);
            if (m_pDevice[i] != 0)
            {
                // Circle only decodes reports while a handler is registered.
                m_pDevice[i]->RegisterStatusHandler(handlers[i]);
            }
        }
    }
}

unsigned Gamepad::Buttons(unsigned port) const
{
    return port < MAX_PADS ? s_buttons[port] : 0;
}

unsigned Gamepad::MenuButtons(void) const
{
    return s_buttons[0] | s_buttons[1];
}
```

Keep the existing `#include "gamepad.h"`, `#include "joypad_map.h"`, and the
four `static_assert` lines tying `GP_*` to `GamePadButton*` at the top of the
file unchanged.

- [ ] **Step 5: Build the kernel to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors or warnings. (Existing callers use
`Buttons()` with the default port, so they still compile.)

- [ ] **Step 6: Commit**

```bash
git add src/input/gamepad.h src/input/gamepad.cpp test/test_joypad.cpp
git commit -m "Input: two-pad Gamepad with per-port cache + MenuButtons"
```

---

## Task 2: Route Player 2 to the core

Enable the second Genesis port and feed it the second pad.

**Files:**
- Modify: `src/libretro/callbacks.cpp`
- Modify: `src/kernel.cpp`

**Interfaces:**
- Consumes: `Gamepad::Buttons(unsigned port)` (Task 1); `joypad_state` (unchanged).

- [ ] **Step 1: Serve both ports in `input_state_cb`**

In `src/libretro/callbacks.cpp`, replace the `input_state_cb` body:

```cpp
int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                       unsigned id)
{
    (void)index;
    if (port != 0 || device != RETRO_DEVICE_JOYPAD || g_gamepad == 0)
    {
        return 0;
    }
    return joypad_state(g_gamepad->Buttons(), id);
}
```

with:

```cpp
int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                       unsigned id)
{
    (void)index;
    if (port >= Gamepad::MAX_PADS || device != RETRO_DEVICE_JOYPAD ||
        g_gamepad == 0)
    {
        return 0;
    }
    return joypad_state(g_gamepad->Buttons(port), id);
}
```

(`callbacks.cpp` already includes `../input/gamepad.h` via its existing
`#include`s for `g_gamepad`; `Gamepad::MAX_PADS` is therefore visible. If the
build reports `Gamepad` is incomplete here, add `#include "../input/gamepad.h"`
at the top of `callbacks.cpp`.)

- [ ] **Step 2: Enable the second controller port in the kernel**

In `src/kernel.cpp`, replace:

```cpp
		retro_set_controller_port_device (1, RETRO_DEVICE_NONE);
```

with:

```cpp
		retro_set_controller_port_device (1, RETRO_DEVICE_MDPAD_6B);
```

- [ ] **Step 3: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 4: Commit**

```bash
git add src/libretro/callbacks.cpp src/kernel.cpp
git commit -m "Input: route Player 2 pad to the second Genesis port"
```

---

## Task 3: Either controller drives menus + pause hotkey

Switch UI navigation and the pause hotkey to the combined `MenuButtons()`.

**Files:**
- Modify: `src/kernel.cpp`
- Modify: `src/menu/rom_menu.cpp`
- Modify: `src/menu/pause_menu.cpp`
- Modify: `src/menu/settings_screen.cpp`

**Interfaces:**
- Consumes: `Gamepad::MenuButtons()` (Task 1).

- [ ] **Step 1: Use MenuButtons for the kernel pause hotkey**

In `src/kernel.cpp`, in the play loop, replace:

```cpp
			m_Gamepad.Poll ();
			unsigned now     = m_Gamepad.Buttons ();
```

with:

```cpp
			m_Gamepad.Poll ();
			unsigned now     = m_Gamepad.MenuButtons ();
```

And replace the post-menu resync:

```cpp
				prevBtns = m_Gamepad.Buttons ();          // resync after the menu
```

with:

```cpp
				prevBtns = m_Gamepad.MenuButtons ();       // resync after the menu
```

- [ ] **Step 2: Use MenuButtons in the ROM browser**

In `src/menu/rom_menu.cpp`, replace:

```cpp
        unsigned now = m_pGamepad->Buttons();
```

with:

```cpp
        unsigned now = m_pGamepad->MenuButtons();
```

- [ ] **Step 3: Use MenuButtons in the pause menu**

In `src/menu/pause_menu.cpp`, replace **every** `m_pGamepad->Buttons()` with
`m_pGamepad->MenuButtons()` (7 occurrences: lines reading the previous/now state
and the post-action resyncs).

Run this to confirm none remain afterward:

Run: `grep -n "m_pGamepad->Buttons()" src/menu/pause_menu.cpp`
Expected: no output.

- [ ] **Step 4: Use MenuButtons in the settings screen**

In `src/menu/settings_screen.cpp`, replace both `m_pGamepad->Buttons()`
occurrences with `m_pGamepad->MenuButtons()`.

Run: `grep -n "m_pGamepad->Buttons()" src/menu/settings_screen.cpp`
Expected: no output.

- [ ] **Step 5: Build and run the full host test suite**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

Run: `cd test && make run`
Expected: all suites pass, including `All joypad tests passed`.

- [ ] **Step 6: Commit**

```bash
git add src/kernel.cpp src/menu/rom_menu.cpp src/menu/pause_menu.cpp src/menu/settings_screen.cpp
git commit -m "Input: either controller drives menus + pause hotkey"
```

---

## Hardware Verification (manual, after Task 3)

Not a code task — perform on the Pi:

- [ ] Plug in two USB pads. In a two-player game (e.g. Sonic 2 2P, Streets of Rage), confirm pad 1 controls Player 1 and pad 2 controls Player 2 independently.
- [ ] Confirm either pad navigates the ROM browser, pause menu, and settings, and either opens the pause menu via Start+Select.
- [ ] With only one pad connected, confirm it plays as Player 1 as before.

---

## Self-Review Notes

- **Spec coverage:** two-pad `Gamepad` with per-port cache + dedicated handlers + `decode_buttons` (Task 1); `Buttons(port)` / `MenuButtons()` (Task 1); ports 0 & 1 via unchanged `joypad_state` + port 1 = `MDPAD_6B` (Task 2); either-controller menus/hotkey (Task 3); combined-pad host assertion (Task 1); hardware checklist incl. one-pad fallback. Multitap/remap/hotplug-removal are out of scope per the spec — no task. First-pad=P1 assignment is the `GetDevice("upad", i+1)` ordering in Task 1.
- **Naming:** class stays `Gamepad` (extended in place) rather than `Gamepads` — documented in the Naming note; behavior is per spec.
- **Type consistency:** `MAX_PADS`, `Buttons(unsigned port)`, `MenuButtons()` defined in Task 1 are used unchanged in Tasks 2–3; `IsPresent(unsigned port)` default keeps any legacy call valid (none exist today).
- **Green builds:** Task 1's default `port = 0` keeps existing `Buttons()` callers compiling, so every task ends with a clean full `make`.
