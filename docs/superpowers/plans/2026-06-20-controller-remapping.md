# Controller Remapping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the user remap the 8 Genesis action buttons to physical pad buttons, per player, via a Controls sub-screen; persisted in `SD:/settings.txt`.

**Architecture:** A pure `PadButton`/`ButtonMap` model in settings (default reproduces today's mapping); `joypad_state` becomes table-driven taking a `ButtonMap`; `input_state_cb` routes each port to its map via `g_map0`/`g_map1`; a Controls sub-screen (P1/P2 + 8 cycle rows) reached from the Settings screen edits the maps live.

**Tech Stack:** C++ (bare-metal, Circle), Genesis-Plus-GX-Wide libretro core, host-side g++ unit tests.

## Global Constraints

- `settings.{h,cpp}` stays free of `<libretro.h>`/Circle (host-testable). `PadButton`/`ButtonMap` are pure types defined there.
- Genesis button index order is fixed: **0=A, 1=B, 2=C, 3=X, 4=Y, 5=Z, 6=Start, 7=Mode**. `ButtonMap.b[i]` = physical button driving Genesis button `i`.
- Default map (reproduces current behavior exactly): A→Y, B→B, C→A, X→L, Y→X, Z→R, Start→Start, Mode→Select.
- Core's fixed Genesis←libretro ids: A←`JOYPAD_Y`, B←`JOYPAD_B`, C←`JOYPAD_A`, X←`JOYPAD_L`, Y←`JOYPAD_X`, Z←`JOYPAD_R`, Start←`JOYPAD_START`, Mode←`JOYPAD_SELECT`; D-pad fixed, not remappable.
- `pad_bit`: A→GP_A, B→GP_B, X→GP_X, Y→GP_Y, L→GP_LB, R→GP_RB, Start→GP_START, Select→GP_SELECT.
- File keys `controller_1_map` / `controller_2_map`, comma-encoded 8 tokens (`a,b,x,y,l,r,start,select`) in Genesis order. Malformed/short/bad token → default map.
- Build `-Wall -Wextra`; member-init order matches declaration order.

---

## File Structure

- `src/settings/settings.h` / `.cpp` — `PadButton`, `ButtonMap`, `Settings.map1/map2`, `pad_button_token`, `parse_button_map`, parse/serialize of the two keys.
- `src/input/joypad_map.h` / `.cpp` — `pad_bit`; `joypad_state(buttons, id, map)`; Genesis-index↔libretro-id table.
- `src/libretro/callbacks.h` / `.cpp` — `g_map0`/`g_map1`; per-port routing in `input_state_cb`.
- `src/kernel.h` / `.cpp` — point `g_map0/1` at the settings maps; own + wire `ControlsScreen`.
- `src/menu/controls_screen.h` / `.cpp` — new sub-screen (P1/P2 + 8 rows).
- `src/menu/settings_screen.h` / `.cpp` — `Controls...` row opening the sub-screen.
- `test/test_settings.cpp`, `test/test_joypad.cpp` — coverage.
- `Makefile` — add `controls_screen.o`.

---

## Task 1: Button-map model + storage

**Files:**
- Modify: `src/settings/settings.h`, `src/settings/settings.cpp`, `test/test_settings.cpp`

**Interfaces:**
- Produces:
  - `enum class PadButton { A, B, X, Y, L, R, Start, Select };`
  - `struct ButtonMap { PadButton b[8]; ButtonMap(); };` (default ctor = the default map)
  - `Settings` gains `ButtonMap map1, map2;`
  - `const char *pad_button_token(PadButton p);`
  - `ButtonMap parse_button_map(const char *val);`
  - File keys `controller_1_map` / `controller_2_map`.

- [ ] **Step 1: Write the failing test additions**

In `test/test_settings.cpp`, add before `printf("All settings tests passed\n");`:

```cpp
    // PadButton tokens.
    assert(strcmp(pad_button_token(PadButton::A), "a") == 0);
    assert(strcmp(pad_button_token(PadButton::L), "l") == 0);
    assert(strcmp(pad_button_token(PadButton::Select), "select") == 0);

    // Default map reproduces current behavior.
    Settings dms;
    assert(dms.map1.b[0] == PadButton::Y);   // Genesis A -> physical Y
    assert(dms.map1.b[2] == PadButton::A);   // Genesis C -> physical A
    assert(dms.map2.b[7] == PadButton::Select);

    // Parse a valid map; round-trips.
    ButtonMap pm = parse_button_map("a,b,x,y,l,r,start,select");
    assert(pm.b[0] == PadButton::A && pm.b[7] == PadButton::Select);
    // Bad token -> default.
    assert(parse_button_map("a,b,x,y,l,r,start,nope").b[0] == PadButton::Y);
    // Wrong count -> default.
    assert(parse_button_map("a,b,c").b[0] == PadButton::Y);

    // Settings round-trip with a custom map.
    Settings cs; cs.map1.b[0] = PadButton::A;
    char cbuf[512];
    serialize_settings(cs, cbuf, sizeof cbuf);
    assert(parse_settings(cbuf).map1.b[0] == PadButton::A);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_settings`
Expected: FAIL — `'PadButton' has not been declared`.

- [ ] **Step 3: Add the model to `settings.h`**

In `src/settings/settings.h`, after the `enum class MenuHotkey ...` line, add:

```cpp
enum class PadButton { A, B, X, Y, L, R, Start, Select };

// Physical pad button driving each Genesis button, indexed
// 0=A,1=B,2=C,3=X,4=Y,5=Z,6=Start,7=Mode. Default ctor reproduces the
// historical mapping (Genesis A<-Y, B<-B, C<-A, X<-L, Y<-X, Z<-R,
// Start<-Start, Mode<-Select).
struct ButtonMap
{
    PadButton b[8];
    ButtonMap(void)
    {
        b[0] = PadButton::Y; b[1] = PadButton::B;
        b[2] = PadButton::A; b[3] = PadButton::L;
        b[4] = PadButton::X; b[5] = PadButton::R;
        b[6] = PadButton::Start; b[7] = PadButton::Select;
    }
};
```

In `Settings`, add the two map fields after `menu_hotkey`:

```cpp
    MenuHotkey menu_hotkey; // controller combo that opens the pause menu
    ButtonMap  map1;        // Player 1 button map
    ButtonMap  map2;        // Player 2 button map
```

(They default-construct; no init-list entry needed.)

After the `menu_hotkey_file_value` declaration, add:

```cpp
// Physical button token for the settings file ("a"|"b"|"x"|"y"|"l"|"r"|
// "start"|"select").
const char *pad_button_token(PadButton p);

// Parse a comma-encoded 8-token button map ("a,b,x,y,l,r,start,select" in
// Genesis order). Any bad token or wrong count yields the default map.
ButtonMap parse_button_map(const char *val);
```

- [ ] **Step 4: Implement tokens + parser in `settings.cpp`**

In `src/settings/settings.cpp`, add near the other helpers (above `serialize_settings`):

```cpp
const char *pad_button_token(PadButton p)
{
    switch (p)
    {
    case PadButton::A:      return "a";
    case PadButton::B:      return "b";
    case PadButton::X:      return "x";
    case PadButton::Y:      return "y";
    case PadButton::L:      return "l";
    case PadButton::R:      return "r";
    case PadButton::Start:  return "start";
    case PadButton::Select: return "select";
    }
    return "a";
}

static int pad_button_from_token(const char *t)
{
    if (ieq(t, "a"))      return (int) PadButton::A;
    if (ieq(t, "b"))      return (int) PadButton::B;
    if (ieq(t, "x"))      return (int) PadButton::X;
    if (ieq(t, "y"))      return (int) PadButton::Y;
    if (ieq(t, "l"))      return (int) PadButton::L;
    if (ieq(t, "r"))      return (int) PadButton::R;
    if (ieq(t, "start"))  return (int) PadButton::Start;
    if (ieq(t, "select")) return (int) PadButton::Select;
    return -1;
}

ButtonMap parse_button_map(const char *val)
{
    PadButton tmp[8];
    int n = 0;
    char tok[16];
    int ti = 0;
    for (const char *p = val; ; p++)
    {
        char c = *p;
        if (c == ',' || c == '\0')
        {
            tok[ti] = '\0';
            if (n >= 8) return ButtonMap();          // too many tokens
            int pb = pad_button_from_token(tok);
            if (pb < 0) return ButtonMap();          // bad token
            tmp[n++] = (PadButton) pb;
            ti = 0;
            if (c == '\0') break;
        }
        else if (ti < (int) sizeof(tok) - 1)
        {
            tok[ti++] = c;
        }
    }
    if (n != 8) return ButtonMap();                  // wrong count
    ButtonMap m;
    for (int i = 0; i < 8; i++) m.b[i] = tmp[i];
    return m;
}

// Append a button map as 8 comma-separated tokens.
static void append_map(char *out, size_t out_size, const ButtonMap &m)
{
    for (int i = 0; i < 8; i++)
    {
        if (i) appendz(out, out_size, ",");
        appendz(out, out_size, pad_button_token(m.b[i]));
    }
}
```

- [ ] **Step 5: Parse the two keys**

In `src/settings/settings.cpp`, in `parse_settings`, add before the
`// unknown keys: ignored` comment:

```cpp
        else if (ieq(key, "controller_1_map"))
            s.map1 = parse_button_map(val);
        else if (ieq(key, "controller_2_map"))
            s.map2 = parse_button_map(val);
```

- [ ] **Step 6: Serialize the two keys**

In `src/settings/settings.cpp`, in `serialize_settings`, replace the
`menu_hotkey` tail:

```cpp
    appendz(out, out_size, "\nmenu_hotkey=");
    appendz(out, out_size, menu_hotkey_file_value(s.menu_hotkey));
    appendz(out, out_size, "\n");
```

with:

```cpp
    appendz(out, out_size, "\nmenu_hotkey=");
    appendz(out, out_size, menu_hotkey_file_value(s.menu_hotkey));
    appendz(out, out_size, "\ncontroller_1_map=");
    append_map(out, out_size, s.map1);
    appendz(out, out_size, "\ncontroller_2_map=");
    append_map(out, out_size, s.map2);
    appendz(out, out_size, "\n");
```

- [ ] **Step 7: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: `All settings tests passed`

- [ ] **Step 8: Build the kernel**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 9: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: PadButton/ButtonMap model + controller_N_map storage"
```

---

## Task 2: Table-driven joypad_state + per-port routing

**Files:**
- Modify: `src/input/joypad_map.h`, `src/input/joypad_map.cpp`
- Modify: `src/libretro/callbacks.h`, `src/libretro/callbacks.cpp`
- Modify: `src/kernel.cpp`
- Modify: `test/test_joypad.cpp`

**Interfaces:**
- Consumes: `ButtonMap`/`PadButton` (Task 1); `Settings.map1/map2` (Task 1).
- Produces:
  - `unsigned pad_bit(PadButton p);`
  - `int16_t joypad_state(unsigned buttons, unsigned retro_id, const ButtonMap &map);`
  - `extern const ButtonMap *g_map0, *g_map1;` (callbacks)

- [ ] **Step 1: Rewrite `test/test_joypad.cpp` (the signature changes)**

Overwrite `test/test_joypad.cpp` with:

```cpp
#include "../src/input/joypad_map.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    ButtonMap dm;   // default map (reproduces historical behavior)

    // Single buttons map correctly under the default map.
    assert(joypad_state(GP_A,      RETRO_DEVICE_ID_JOYPAD_A, dm)      == 1);
    assert(joypad_state(GP_B,      RETRO_DEVICE_ID_JOYPAD_B, dm)      == 1);
    assert(joypad_state(GP_X,      RETRO_DEVICE_ID_JOYPAD_X, dm)      == 1);
    assert(joypad_state(GP_Y,      RETRO_DEVICE_ID_JOYPAD_Y, dm)      == 1);
    assert(joypad_state(GP_LB,     RETRO_DEVICE_ID_JOYPAD_L, dm)      == 1);
    assert(joypad_state(GP_RB,     RETRO_DEVICE_ID_JOYPAD_R, dm)      == 1);
    assert(joypad_state(GP_START,  RETRO_DEVICE_ID_JOYPAD_START, dm)  == 1);
    assert(joypad_state(GP_SELECT, RETRO_DEVICE_ID_JOYPAD_SELECT, dm) == 1);
    assert(joypad_state(GP_UP,     RETRO_DEVICE_ID_JOYPAD_UP, dm)     == 1);
    assert(joypad_state(GP_DOWN,   RETRO_DEVICE_ID_JOYPAD_DOWN, dm)   == 1);
    assert(joypad_state(GP_LEFT,   RETRO_DEVICE_ID_JOYPAD_LEFT, dm)   == 1);
    assert(joypad_state(GP_RIGHT,  RETRO_DEVICE_ID_JOYPAD_RIGHT, dm)  == 1);

    // A pressed button does not register as a different one.
    assert(joypad_state(GP_A, RETRO_DEVICE_ID_JOYPAD_B, dm) == 0);

    // Multiple buttons at once resolve independently.
    unsigned combo = GP_A | GP_DOWN | GP_RB;
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_A, dm)    == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_DOWN, dm) == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_R, dm)    == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_B, dm)    == 0);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_UP, dm)   == 0);

    // Empty bitmask and unmapped id.
    assert(joypad_state(0,          RETRO_DEVICE_ID_JOYPAD_A, dm) == 0);
    assert(joypad_state(0xFFFFFFFF, 999u, dm)                     == 0);

    // JOYPAD_MASK returns a bitmask of pressed button ids.
    int16_t m = joypad_state(GP_A | GP_START | GP_DOWN, RETRO_DEVICE_ID_JOYPAD_MASK, dm);
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_A));
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_START));
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));
    assert(!(m & (1 << RETRO_DEVICE_ID_JOYPAD_B)));
    assert(!(m & (1 << RETRO_DEVICE_ID_JOYPAD_UP)));
    assert(joypad_state(0, RETRO_DEVICE_ID_JOYPAD_MASK, dm) == 0);

    // Two pads combine for menu navigation as a bitwise OR.
    unsigned combined = (GP_UP) | (GP_START);
    assert(joypad_state(combined, RETRO_DEVICE_ID_JOYPAD_UP, dm));
    assert(joypad_state(combined, RETRO_DEVICE_ID_JOYPAD_START, dm));

    // Remapping: physical A now drives Genesis A (id JOYPAD_Y).
    ButtonMap rm;
    rm.b[0] = PadButton::A;
    assert(joypad_state(GP_A, RETRO_DEVICE_ID_JOYPAD_Y, rm) == 1);
    assert(joypad_state(GP_Y, RETRO_DEVICE_ID_JOYPAD_Y, rm) == 0);
    // D-pad stays fixed regardless of the map.
    assert(joypad_state(GP_UP, RETRO_DEVICE_ID_JOYPAD_UP, rm) == 1);

    // pad_bit mapping.
    assert(pad_bit(PadButton::A) == GP_A);
    assert(pad_bit(PadButton::L) == GP_LB);
    assert(pad_bit(PadButton::Select) == GP_SELECT);

    // Menu-hotkey preset -> button bitmask.
    assert(hotkey_mask(MenuHotkey::StartSelect) == (GP_START | GP_SELECT));
    assert(hotkey_mask(MenuHotkey::StartA)      == (GP_START | GP_A));
    assert(hotkey_mask(MenuHotkey::StartB)      == (GP_START | GP_B));
    assert(hotkey_mask(MenuHotkey::LR)          == (GP_LB | GP_RB));

    printf("All joypad tests passed\n");
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_joypad`
Expected: FAIL — `joypad_state` 3-arg / `pad_bit` not declared.

- [ ] **Step 3: Update the `joypad_map.h` declarations**

In `src/input/joypad_map.h`, replace:

```cpp
int16_t joypad_state(unsigned buttons, unsigned retro_id);
```

with:

```cpp
// The GP_* bit for a physical pad button.
unsigned pad_bit(PadButton p);

// buttons: physical GP_* bitmask. retro_id: RETRO_DEVICE_ID_JOYPAD_*. map: the
// active button map (action buttons are remappable; D-pad is fixed). Returns 1
// if the (mapped) button is pressed, else 0; for JOYPAD_MASK returns the id
// bitmask.
int16_t joypad_state(unsigned buttons, unsigned retro_id, const ButtonMap &map);
```

- [ ] **Step 4: Rewrite `joypad_state` and add `pad_bit` in `joypad_map.cpp`**

In `src/input/joypad_map.cpp`, replace the entire `joypad_state` function with:

```cpp
unsigned pad_bit(PadButton p)
{
    switch (p)
    {
    case PadButton::A:      return GP_A;
    case PadButton::B:      return GP_B;
    case PadButton::X:      return GP_X;
    case PadButton::Y:      return GP_Y;
    case PadButton::L:      return GP_LB;
    case PadButton::R:      return GP_RB;
    case PadButton::Start:  return GP_START;
    case PadButton::Select: return GP_SELECT;
    }
    return 0;
}

// libretro id for each Genesis button index (A,B,C,X,Y,Z,Start,Mode).
static const unsigned k_action_id[8] = {
    RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B,
    RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_L,
    RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_R,
    RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT
};

int16_t joypad_state(unsigned buttons, unsigned retro_id, const ButtonMap &map)
{
    if (retro_id == RETRO_DEVICE_ID_JOYPAD_MASK)
    {
        static const unsigned ids[] = {
            RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_DOWN,
            RETRO_DEVICE_ID_JOYPAD_LEFT,  RETRO_DEVICE_ID_JOYPAD_RIGHT,
            RETRO_DEVICE_ID_JOYPAD_A,     RETRO_DEVICE_ID_JOYPAD_B,
            RETRO_DEVICE_ID_JOYPAD_X,     RETRO_DEVICE_ID_JOYPAD_Y,
            RETRO_DEVICE_ID_JOYPAD_L,     RETRO_DEVICE_ID_JOYPAD_R,
            RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT
        };
        int16_t ret = 0;
        for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++)
        {
            if (joypad_state(buttons, ids[i], map))
            {
                ret |= (int16_t) (1 << ids[i]);
            }
        }
        return ret;
    }

    // D-pad: fixed directional mapping.
    unsigned mask = 0;
    switch (retro_id)
    {
    case RETRO_DEVICE_ID_JOYPAD_UP:    mask = GP_UP;    break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:  mask = GP_DOWN;  break;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:  mask = GP_LEFT;  break;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT: mask = GP_RIGHT; break;
    default: break;
    }
    if (mask) return (buttons & mask) ? 1 : 0;

    // Action buttons: remappable via the ButtonMap.
    for (int i = 0; i < 8; i++)
    {
        if (k_action_id[i] == retro_id)
        {
            return (buttons & pad_bit(map.b[i])) ? 1 : 0;
        }
    }

    return 0;
}
```

(`hotkey_mask` below it is unchanged.)

- [ ] **Step 5: Run the joypad test to verify it passes**

Run: `cd test && make test_joypad && ./test_joypad`
Expected: `All joypad tests passed`

- [ ] **Step 6: Declare the per-port map globals in `callbacks.h`**

In `src/libretro/callbacks.h`, after the `extern Gamepad *g_gamepad;` block, add:

```cpp
// Per-port button maps (point into the kernel's Settings). input_state_cb reads
// them so the core sees the user's remapping.
struct ButtonMap;
extern const ButtonMap *g_map0;
extern const ButtonMap *g_map1;
```

- [ ] **Step 7: Define the globals and route per port in `callbacks.cpp`**

In `src/libretro/callbacks.cpp`, after the existing `g_gamepad` definition
(`Gamepad *g_gamepad = 0;` or similar), add:

```cpp
const ButtonMap *g_map0 = 0;
const ButtonMap *g_map1 = 0;
```

Replace `input_state_cb`:

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
    const ButtonMap *map = (port == 0) ? g_map0 : g_map1;
    if (map == 0)
    {
        return 0;
    }
    return joypad_state(g_gamepad->Buttons(port), id, *map);
}
```

(`callbacks.cpp` already includes `../input/joypad_map.h`, which provides the
full `ButtonMap` type via `settings.h`.)

- [ ] **Step 8: Point the globals at the settings maps in `kernel.cpp`**

In `src/kernel.cpp`, after the boot settings-apply block, replace:

```cpp
	g_region_value = region_core_value (m_Settings.region);
```

with:

```cpp
	g_region_value = region_core_value (m_Settings.region);
	g_map0 = &m_Settings.map1;
	g_map1 = &m_Settings.map2;
```

- [ ] **Step 9: Build and run the full host suite**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

Run: `cd test && make run`
Expected: all suites pass (`All joypad tests passed`, `All settings tests passed`).

- [ ] **Step 10: Commit**

```bash
git add src/input/joypad_map.h src/input/joypad_map.cpp src/libretro/callbacks.h src/libretro/callbacks.cpp src/kernel.cpp test/test_joypad.cpp
git commit -m "Input: table-driven joypad_state + per-port button maps"
```

---

## Task 3: Controls sub-screen

**Files:**
- Create: `src/menu/controls_screen.h`, `src/menu/controls_screen.cpp`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `Settings.map1/map2`, `PadButton`, `pad_button_token` (Task 1); `Gamepad::MenuButtons` / `Poll`; `SettingsStore::Save`.
- Produces: `class ControlsScreen { ControlsScreen(TextCanvas*, Gamepad*, CUSBHCIDevice*, Settings*, SettingsStore*); void Run(); };`

- [ ] **Step 1: Write the header**

Create `src/menu/controls_screen.h`:

```cpp
//
// src/menu/controls_screen.h
//
// Bare Metal Sega Genesis
// Button-remapping sub-screen: pick a player and reassign each Genesis action
// button to a physical pad button. Reached from the Settings screen.
//

#ifndef _menu_controls_screen_h
#define _menu_controls_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"

class ControlsScreen
{
public:
    ControlsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore);
    void Run(void);   // returns when the user backs out (B)

private:
    void Render(int player, int selected);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
};

#endif
```

- [ ] **Step 2: Write the implementation**

Create `src/menu/controls_screen.cpp`:

```cpp
//
// src/menu/controls_screen.cpp
//
// Bare Metal Sega Genesis
// See controls_screen.h.
//

#include "controls_screen.h"
#include "../input/joypad_map.h"   // GP_* via header (GP_UP etc. for nav)
#include <circle/timer.h>

#define NUM_BTN  8                 // Genesis action buttons
#define NUM_ROWS (NUM_BTN + 1)     // + the Player selector row

static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

static const char *const GEN_LABEL[NUM_BTN] =
    { "A", "B", "C", "X", "Y", "Z", "Start", "Mode" };

static const char *phys_label(PadButton p)
{
    switch (p)
    {
    case PadButton::A:      return "A";
    case PadButton::B:      return "B";
    case PadButton::X:      return "X";
    case PadButton::Y:      return "Y";
    case PadButton::L:      return "L";
    case PadButton::R:      return "R";
    case PadButton::Start:  return "Start";
    case PadButton::Select: return "Select";
    }
    return "?";
}

ControlsScreen::ControlsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore)
{
}

void ControlsScreen::Render(int player, int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2;
    int boxW = cw * 34, boxH = ch * (NUM_ROWS + 4);

    const ButtonMap &map = (player == 0) ? m_pSettings->map1 : m_pSettings->map2;

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "CONTROLS", WHITE, BOX);

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  ty  = boxY + ch * (i + 3);
        bool sel = (i == selected);
        u16  fg  = sel ? SELFG : WHITE;
        u16  bg  = sel ? SELBG : BOX;
        if (sel) m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        m_pCanvas->DrawText(boxX + cw, ty, sel ? ">" : " ", fg, bg);

        if (i == 0)
        {
            m_pCanvas->DrawText(boxX + cw * 3,  ty, "Player", fg, bg);
            m_pCanvas->DrawText(boxX + cw * 13, ty, player == 0 ? "< 1 >" : "< 2 >",
                                fg, bg);
        }
        else
        {
            int btn = i - 1;
            char val[12];
            val[0] = '<'; val[1] = ' ';
            const char *pl = phys_label(map.b[btn]);
            int k = 2;
            for (int j = 0; pl[j] && k < 9; j++) val[k++] = pl[j];
            val[k++] = ' '; val[k++] = '>'; val[k] = '\0';
            m_pCanvas->DrawText(boxX + cw * 3,  ty, GEN_LABEL[btn], fg, bg);
            m_pCanvas->DrawText(boxX + cw * 13, ty, val, fg, bg);
        }
    }

    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 3),
                        "Left/Right: change   B: back", WHITE, BOX);
}

void ControlsScreen::Run(void)
{
    int player   = 0;
    int selected = 0;
    Render(player, selected);

    unsigned prev = m_pGamepad->MenuButtons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->MenuButtons();
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)
        {
            selected = (selected + NUM_ROWS - 1) % NUM_ROWS;
            Render(player, selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = (selected + 1) % NUM_ROWS;
            Render(player, selected);
        }

        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            if (selected == 0)
            {
                player ^= 1;                     // toggle P1/P2
            }
            else
            {
                ButtonMap &map = (player == 0) ? m_pSettings->map1
                                               : m_pSettings->map2;
                int btn = selected - 1;
                int v = (int) map.b[btn] + dir;
                if (v < 0) v = 7;
                if (v > 7) v = 0;
                map.b[btn] = (PadButton) v;
                m_pStore->Save(*m_pSettings);
            }
            Render(player, selected);
        }

        if (pressed & GP_B)
        {
            return;
        }

        CTimer::SimpleMsDelay(16);
    }
}
```

- [ ] **Step 3: Add the object to the `Makefile`**

In `Makefile`, add to `OBJS` after `src/menu/settings_screen.o`:

```makefile
       src/menu/controls_screen.o \
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors. (The screen is linked but not yet
opened — wired in Task 4.)

- [ ] **Step 5: Commit**

```bash
git add src/menu/controls_screen.h src/menu/controls_screen.cpp Makefile
git commit -m "Settings: Controls remapping sub-screen"
```

---

## Task 4: Open Controls from the Settings screen + kernel wiring

**Files:**
- Modify: `src/menu/settings_screen.h`, `src/menu/settings_screen.cpp`
- Modify: `src/kernel.h`, `src/kernel.cpp`

**Interfaces:**
- Consumes: `ControlsScreen` (Task 3).
- Produces: `SettingsScreen` constructor gains a `ControlsScreen *pControls` parameter (appended last).

- [ ] **Step 1: Add ControlsScreen to the Settings-screen header**

In `src/menu/settings_screen.h`, after `#include "../audio/audio_driver.h"`, add:

```cpp
class ControlsScreen;
```

Change the constructor declaration to append `ControlsScreen *pControls`:

```cpp
    SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay,
                   AudioDriver *pAudio, ControlsScreen *pControls);
```

In the `private:` member list, after `const char *m_pRomPath;`, add:

```cpp
    ControlsScreen *m_pControls;
```

- [ ] **Step 2: Wire the constructor, grow rows, render + open**

In `src/menu/settings_screen.cpp`, add the include after the existing includes:

```cpp
#include "controls_screen.h"
```

Update the constructor init list (append `m_pControls`):

```cpp
    m_pAudio(pAudio), m_pRomPath(0), m_pControls(pControls)
```

and the constructor signature to match:

```cpp
SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay,
                               AudioDriver *pAudio, ControlsScreen *pControls)
```

Change `#define NUM_ROWS 7` to:

```cpp
#define NUM_ROWS 8
```

In `Render`, append the `Controls...` label/value (an action row, blank value):

```cpp
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Controls..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, "" };
```

In `Run`, after the `if (dir != 0) { ... }` block and before the `if (pressed & GP_B)` check, add an action handler for the Controls row:

```cpp
        if ((pressed & GP_START) && selected == 7)
        {
            m_pControls->Run();
            prev = m_pGamepad->MenuButtons();   // resync after the sub-screen
            Render(selected);
        }
```

- [ ] **Step 3: Declare + construct ControlsScreen in the kernel**

In `src/kernel.h`, add the include after `#include "menu/settings_screen.h"`:

```cpp
#include "menu/controls_screen.h"
```

In the member list, immediately before `SettingsScreen m_SettingsScreen;`, add:

```cpp
	ControlsScreen     m_ControlsScreen; // button-remapping sub-screen
```

- [ ] **Step 4: Wire the kernel constructor**

In `src/kernel.cpp`, in the constructor init list, replace the
`m_SettingsScreen (...)` line with the `ControlsScreen` construction first, then
the updated `SettingsScreen` (order matches declaration order):

```cpp
	m_ControlsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore),
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display, &m_Audio, &m_ControlsScreen),
```

- [ ] **Step 5: Build and run the full host suite**

Run: `make`
Expected: builds to `kernel7.img`, no errors or `-Wreorder` warnings.

Run: `cd test && make run`
Expected: all suites pass.

- [ ] **Step 6: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp src/kernel.h src/kernel.cpp
git commit -m "Settings: open Controls sub-screen from the Settings screen"
```

---

## Hardware Verification (manual, after Task 4)

Not a code task — perform on the Pi:

- [ ] In a game, Settings → Controls... → set Genesis A to physical A; confirm in-game.
- [ ] Switch Player to 2, set a different map; confirm P1 and P2 behave independently in a 2-player game.
- [ ] Power-cycle → confirm both maps persist in `SD:/settings.txt`.
- [ ] Confirm the D-pad still works regardless of remapping.

---

## Self-Review Notes

- **Spec coverage:** `PadButton`/`ButtonMap`/default (Task 1); `pad_bit` + table-driven `joypad_state` + fixed index↔id table (Task 2); per-port routing via `g_map0/1` + kernel wiring (Task 2); comma storage + parse fallback (Task 1); Controls sub-screen with P1/P2 + 8 cycle rows + live save (Task 3); opened from Settings screen (Task 4); host tests for tokens/parse/default/remap/pad_bit + regression of existing default-map behavior (Tasks 1–2); hardware checklist incl. D-pad-still-works and per-player independence. Conflicts allowed (no blocking code). D-pad not remappable (fixed switch).
- **Type consistency:** `ButtonMap`/`PadButton`/`pad_button_token`/`parse_button_map` (Task 1) used in Tasks 2–4; `joypad_state(.,.,map)`/`pad_bit` (Task 2) used by callbacks (Task 2); `g_map0`/`g_map1` (Task 2) set by kernel (Task 2); `ControlsScreen` ctor (Task 3) matches kernel construction (Task 4); `SettingsScreen` 8-arg ctor consistent between Task 4 header/impl/kernel.
- **Green builds:** Task 2 changes the `joypad_state` signature and updates its only caller (callbacks) + the test in the same task; every task ends with a clean full `make`. Task 3's screen links unused until Task 4 opens it.
