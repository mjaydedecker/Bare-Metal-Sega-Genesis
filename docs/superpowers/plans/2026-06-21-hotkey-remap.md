# Remappable Hotkey Bindings + Remap UI Implementation Plan (Spec B2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Spec B1 in-game hotkeys user-configurable: each action binds a (hold, trigger) pair, edited in a remap screen, persisted to settings.txt, with data-driven input suppression.

**Architecture:** `HotkeyBindings` (in `settings.h`) holds six (hold, trigger) `PadButton` pairs. A generalized pure `decode_hotkey(held, pressed, bindings)` plus pure `hotkey_hold_mask()` / `hotkey_conflicts()` live in `src/input/hotkey.*`. Suppression reads a data-driven `g_hotkey_hold_mask` global. A `HotkeyScreen` (mirroring `ControlsScreen`) edits the bindings.

**Tech Stack:** C++ bare-metal Circle (device) + host C++ unit tests via `test/Makefile`.

## Global Constraints

- `src/input/hotkey.*` stays pure/host-testable: `hotkey.h` includes `joypad_map.h` (for `pad_bit`/`hotkey_mask`) and `settings.h` (for `HotkeyBindings`/`PadButton`/`MenuHotkey`); no Circle.
- `HotkeyBindings` lives in `settings.h` (pure). `HK_*` index `i` maps to `InGameAction` value `i+1` (since `InGameAction::None == 0`).
- **hold** ∈ {Select, Start, L, R} (PadButton values 4..7 — contiguous in the enum `{A,B,X,Y,L,R,Start,Select}`); **trigger** ∈ any `PadButton` except the action's hold. No d-pad.
- Defaults reproduce B1 with PadButton triggers: all holds = Select; triggers X/Y/A/B for save/load/HUD/mute, L/R for vol+/vol-.
- Invalid binding on parse (bad token, out-of-set hold, `hold == trigger`) → leave that action's default.
- New kernel `.o`s go in the root `Makefile` `OBJS`. No `Display::Blit`/`Present` changes.
- Host tests: `make -C test run`. Device build: `make`.

---

### Task 1: `HotkeyBindings` model + settings serialize/parse

**Files:**
- Modify: `src/settings/settings.h` (struct/enum + `Settings` member)
- Modify: `src/settings/settings.cpp` (parse + serialize + `parse_hotkey` helper)
- Test: `test/test_settings.cpp`

**Interfaces:**
- Produces: `struct HotkeyBinding { PadButton hold; PadButton trigger; };`, `enum { HK_QUICKSAVE, HK_QUICKLOAD, HK_VOLUP, HK_VOLDOWN, HK_TOGGLEHUD, HK_MUTE, HK_COUNT };`, `struct HotkeyBindings { HotkeyBinding b[HK_COUNT]; ... };`, and `Settings::hotkeys`. Six `hotkey_*` settings keys round-trip.

- [ ] **Step 1: Write the failing test**

In `test/test_settings.cpp`, after the `debug_overlay` block, add:

```cpp
    // Hotkey bindings: defaults.
    Settings hkd = parse_settings("");
    assert(hkd.hotkeys.b[HK_QUICKSAVE].hold    == PadButton::Select);
    assert(hkd.hotkeys.b[HK_QUICKSAVE].trigger == PadButton::X);
    assert(hkd.hotkeys.b[HK_VOLUP].trigger     == PadButton::L);

    // Valid override round-trips.
    Settings hk = parse_settings("hotkey_mute=start+a\n");
    assert(hk.hotkeys.b[HK_MUTE].hold    == PadButton::Start);
    assert(hk.hotkeys.b[HK_MUTE].trigger == PadButton::A);
    char hkbuf[1024];
    serialize_settings(hk, hkbuf, sizeof hkbuf);
    Settings hkrt = parse_settings(hkbuf);
    assert(hkrt.hotkeys.b[HK_MUTE].hold    == PadButton::Start);
    assert(hkrt.hotkeys.b[HK_MUTE].trigger == PadButton::A);

    // Invalid -> default for that action: out-of-set hold (A), hold==trigger, junk.
    assert(parse_settings("hotkey_mute=a+x\n").hotkeys.b[HK_MUTE].hold
           == PadButton::Select);                       // A not a safe hold
    assert(parse_settings("hotkey_mute=select+select\n").hotkeys.b[HK_MUTE].trigger
           == PadButton::B);                            // hold==trigger rejected
    assert(parse_settings("hotkey_mute=nonsense\n").hotkeys.b[HK_MUTE].trigger
           == PadButton::B);                            // junk rejected
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -C test test_settings`
Expected: FAIL — `HotkeyBinding`/`HK_*`/`Settings::hotkeys` undefined.

- [ ] **Step 3: Add the model to `src/settings/settings.h`**

After the `ButtonMap` definition (the `struct ButtonMap { ... };` block) and before `struct Settings`, add:

```cpp
// In-game hotkey actions. Index maps to InGameAction value (index+1).
enum { HK_QUICKSAVE, HK_QUICKLOAD, HK_VOLUP, HK_VOLDOWN, HK_TOGGLEHUD, HK_MUTE,
       HK_COUNT };

// One in-game hotkey: hold a "modifier" button, press a "trigger" button.
struct HotkeyBinding { PadButton hold; PadButton trigger; };

struct HotkeyBindings
{
    HotkeyBinding b[HK_COUNT];

    HotkeyBindings(void)
    {
        b[HK_QUICKSAVE] = { PadButton::Select, PadButton::X };
        b[HK_QUICKLOAD] = { PadButton::Select, PadButton::Y };
        b[HK_VOLUP]     = { PadButton::Select, PadButton::L };
        b[HK_VOLDOWN]   = { PadButton::Select, PadButton::R };
        b[HK_TOGGLEHUD] = { PadButton::Select, PadButton::A };
        b[HK_MUTE]      = { PadButton::Select, PadButton::B };
    }
};
```

Add the member to `struct Settings` (after `debug_overlay;`):

```cpp
    bool       debug_overlay;  // on-screen diagnostics HUD (default off)
    HotkeyBindings hotkeys;    // in-game action hotkey bindings
```

(`hotkeys` is default-constructed — no change to the `Settings` ctor init list needed; a member with a default constructor is initialized automatically.)

- [ ] **Step 4: Add the parse helper + keys + serialize in `src/settings/settings.cpp`**

Near the top of the file (after the existing `static int pad_button_from_token(...)` definition), add a binding parser:

```cpp
// Parse "hold+trigger" (e.g. "select+x") into out. Returns false (caller keeps
// the default) on a bad token, a hold outside {L,R,Start,Select}, or hold==trigger.
static bool parse_hotkey(const char *val, HotkeyBinding *out)
{
    char a[16], b[16];
    int  i = 0;
    const char *p = val;
    while (*p && *p != '+' && i < 15) a[i++] = *p++;
    a[i] = '\0';
    if (*p != '+') return false;
    p++;
    int j = 0;
    while (*p && j < 15) b[j++] = *p++;
    b[j] = '\0';

    int h = pad_button_from_token(a);
    int t = pad_button_from_token(b);
    if (h < 0 || t < 0 || h == t) return false;
    if (h < (int) PadButton::L) return false;   // safe holds: L,R,Start,Select
    out->hold    = (PadButton) h;
    out->trigger = (PadButton) t;
    return true;
}
```

In the parse key chain (after the `debug_overlay` branch), add six branches:

```cpp
        else if (ieq(key, "debug_overlay"))
            s.debug_overlay = truthy(val);
        else if (ieq(key, "hotkey_quicksave")) parse_hotkey(val, &s.hotkeys.b[HK_QUICKSAVE]);
        else if (ieq(key, "hotkey_quickload")) parse_hotkey(val, &s.hotkeys.b[HK_QUICKLOAD]);
        else if (ieq(key, "hotkey_volup"))     parse_hotkey(val, &s.hotkeys.b[HK_VOLUP]);
        else if (ieq(key, "hotkey_voldown"))   parse_hotkey(val, &s.hotkeys.b[HK_VOLDOWN]);
        else if (ieq(key, "hotkey_togglehud")) parse_hotkey(val, &s.hotkeys.b[HK_TOGGLEHUD]);
        else if (ieq(key, "hotkey_mute"))      parse_hotkey(val, &s.hotkeys.b[HK_MUTE]);
        // unknown keys: ignored
```

(`parse_hotkey` returning false leaves the already-default `s.hotkeys.b[...]` untouched.)

In `serialize_settings`, before the final `appendz(out, out_size, "\n");`, add (a small local helper keeps it DRY):

```cpp
    static const char *const HK_KEY[HK_COUNT] = {
        "hotkey_quicksave", "hotkey_quickload", "hotkey_volup",
        "hotkey_voldown", "hotkey_togglehud", "hotkey_mute" };
    for (int i = 0; i < HK_COUNT; i++)
    {
        appendz(out, out_size, "\n");
        appendz(out, out_size, HK_KEY[i]);
        appendz(out, out_size, "=");
        appendz(out, out_size, pad_button_token(s.hotkeys.b[i].hold));
        appendz(out, out_size, "+");
        appendz(out, out_size, pad_button_token(s.hotkeys.b[i].trigger));
    }
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_settings && ./test/test_settings`
Expected: PASS — `All settings tests passed`.

- [ ] **Step 6: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add HotkeyBindings model + hotkey_* keys (parse/serialize + test)"
```

---

### Task 2: data-driven suppression global

**Files:**
- Modify: `src/libretro/callbacks.h` (extern), `src/libretro/callbacks.cpp` (define + use)

**Interfaces:**
- Produces: `extern unsigned g_hotkey_hold_mask;` (default `GP_SELECT`); `input_state_cb` masks port-0 while any of its bits are held.

- [ ] **Step 1: Declare the global in `callbacks.h`**

After the `extern const ButtonMap *g_map1;` line, add:

```cpp
// OR of the GP_* bits of every bound hotkey "hold" button. input_state_cb masks
// player-1 input while any are held (hotkey mode). Set by the kernel.
extern unsigned g_hotkey_hold_mask;
```

- [ ] **Step 2: Define it and use it in `callbacks.cpp`**

Add the definition next to `g_map1` (top of the file):

```cpp
unsigned g_hotkey_hold_mask = GP_SELECT;
```

Replace the B1 literal guard in `input_state_cb`:

```cpp
    unsigned buttons = g_gamepad->Buttons(port);
    if (port == 0 && (buttons & g_hotkey_hold_mask))   // player-1 hotkey mode: mask
    {
        return 0;
    }
    return joypad_state(buttons, id, *map);
```

(`GP_SELECT` is still in scope via `joypad_map.h` for the default initializer.)

- [ ] **Step 3: Build to verify it compiles**

Run: `make`
Expected: builds to completion; `callbacks.o` recompiles cleanly. Behavior unchanged (mask still defaults to `GP_SELECT`).

- [ ] **Step 4: Commit**

```bash
git add src/libretro/callbacks.h src/libretro/callbacks.cpp
git commit -m "Input: data-driven g_hotkey_hold_mask for suppression (default GP_SELECT)"
```

---

### Task 3: generalize `decode_hotkey` + `hotkey_hold_mask` + `hotkey_conflicts`

**Files:**
- Modify: `src/input/hotkey.h`, `src/input/hotkey.cpp`
- Modify: `test/test_hotkey.cpp` (update to 3-arg + new helpers)
- Modify: `src/kernel.cpp` (update the play-loop decode call to 3-arg; set the mask at boot)

**Interfaces:**
- Consumes: `HotkeyBindings`/`HK_*`/`MenuHotkey` (Task 1), `pad_bit`/`hotkey_mask` (joypad_map), `g_hotkey_hold_mask` (Task 2).
- Produces: `InGameAction decode_hotkey(unsigned held, unsigned pressed, const HotkeyBindings &hk);`, `unsigned hotkey_hold_mask(const HotkeyBindings &hk);`, `unsigned hotkey_conflicts(const HotkeyBindings &hk, MenuHotkey menu);`.

- [ ] **Step 1: Update the test `test/test_hotkey.cpp`**

Replace its contents with:

```cpp
#include "../src/input/hotkey.h"
#include "../src/input/joypad_map.h"   // GP_*, hotkey_mask, MenuHotkey
#include "../src/settings/settings.h"  // HotkeyBindings, HK_*, PadButton
#include <assert.h>
#include <stdio.h>

int main(void)
{
    HotkeyBindings hk;   // default bindings: Select + X/Y/A/B and L/R

    // Each default combo fires for its action.
    assert(decode_hotkey(GP_SELECT | GP_X, GP_X, hk) == InGameAction::QuickSave);
    assert(decode_hotkey(GP_SELECT | GP_Y, GP_Y, hk) == InGameAction::QuickLoad);
    assert(decode_hotkey(GP_SELECT | GP_LB, GP_LB, hk) == InGameAction::VolUp);
    assert(decode_hotkey(GP_SELECT | GP_RB, GP_RB, hk) == InGameAction::VolDown);
    assert(decode_hotkey(GP_SELECT | GP_A, GP_A, hk) == InGameAction::ToggleHud);
    assert(decode_hotkey(GP_SELECT | GP_B, GP_B, hk) == InGameAction::Mute);

    // Hold not held / trigger not pressed -> None.
    assert(decode_hotkey(GP_X, GP_X, hk) == InGameAction::None);
    assert(decode_hotkey(GP_SELECT | GP_X, 0, hk) == InGameAction::None);

    // Priority: two actions sharing a combo -> lower index wins.
    HotkeyBindings dup = hk;
    dup.b[HK_MUTE] = { PadButton::Select, PadButton::X };   // same as QuickSave
    assert(decode_hotkey(GP_SELECT | GP_X, GP_X, dup) == InGameAction::QuickSave);

    // hotkey_hold_mask of defaults == GP_SELECT.
    assert(hotkey_hold_mask(hk) == GP_SELECT);

    // No conflicts on defaults, vs every menu preset.
    MenuHotkey presets[4] = { MenuHotkey::StartSelect, MenuHotkey::StartA,
                              MenuHotkey::StartB, MenuHotkey::LR };
    for (int i = 0; i < 4; i++)
        assert(hotkey_conflicts(hk, presets[i]) == 0);

    // Duplicate combo flags both actions.
    unsigned c = hotkey_conflicts(dup, MenuHotkey::StartSelect);
    assert((c & (1u << HK_QUICKSAVE)) && (c & (1u << HK_MUTE)));

    // A combo equal to a menu preset flags that action.
    HotkeyBindings clash = hk;
    clash.b[HK_MUTE] = { PadButton::Start, PadButton::Select };  // == Start+Select
    assert(hotkey_conflicts(clash, MenuHotkey::StartSelect) & (1u << HK_MUTE));

    printf("All hotkey tests passed\n");
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -C test test_hotkey`
Expected: FAIL — `decode_hotkey` arity mismatch / `hotkey_hold_mask`/`hotkey_conflicts` undefined.

- [ ] **Step 3: Update `src/input/hotkey.h`**

Replace the `decode_hotkey` declaration and add the two helpers:

```cpp
#ifndef _input_hotkey_h
#define _input_hotkey_h

#include "../settings/settings.h"   // HotkeyBindings, HK_*, MenuHotkey

enum class InGameAction
{
    None, QuickSave, QuickLoad, VolUp, VolDown, ToggleHud, Mute
};

// Returns the bound action when its hold is in `held` and its trigger is freshly
// in `pressed`; ties resolve by HK_ index order (lower wins). Else None.
InGameAction decode_hotkey(unsigned held, unsigned pressed,
                           const HotkeyBindings &hk);

// OR of pad_bit() of every binding's hold button (for input suppression).
unsigned hotkey_hold_mask(const HotkeyBindings &hk);

// Bit i set if binding i's combo duplicates another binding's, or equals the
// given menu-hotkey preset mask.
unsigned hotkey_conflicts(const HotkeyBindings &hk, MenuHotkey menu);

#endif
```

- [ ] **Step 4: Rewrite `src/input/hotkey.cpp`**

```cpp
//
// src/input/hotkey.cpp
//
// Bare Metal Sega Genesis
// See hotkey.h.
//

#include "hotkey.h"
#include "joypad_map.h"   // pad_bit, hotkey_mask, GP_* bits

InGameAction decode_hotkey(unsigned held, unsigned pressed,
                           const HotkeyBindings &hk)
{
    for (int i = 0; i < HK_COUNT; i++)
    {
        if ((held & pad_bit(hk.b[i].hold)) &&
            (pressed & pad_bit(hk.b[i].trigger)))
        {
            return (InGameAction) (i + 1);   // HK_* index -> InGameAction value
        }
    }
    return InGameAction::None;
}

unsigned hotkey_hold_mask(const HotkeyBindings &hk)
{
    unsigned m = 0;
    for (int i = 0; i < HK_COUNT; i++) m |= pad_bit(hk.b[i].hold);
    return m;
}

unsigned hotkey_conflicts(const HotkeyBindings &hk, MenuHotkey menu)
{
    unsigned combo[HK_COUNT];
    for (int i = 0; i < HK_COUNT; i++)
        combo[i] = pad_bit(hk.b[i].hold) | pad_bit(hk.b[i].trigger);

    unsigned menuMask = hotkey_mask(menu);
    unsigned bits = 0;
    for (int i = 0; i < HK_COUNT; i++)
    {
        if (combo[i] == menuMask) bits |= (1u << i);
        for (int j = 0; j < HK_COUNT; j++)
            if (j != i && combo[i] == combo[j]) bits |= (1u << i);
    }
    return bits;
}
```

- [ ] **Step 5: Update the kernel decode call + boot mask**

In `src/kernel.cpp`, change the play-loop call (from Spec B1):

```cpp
			InGameAction act = decode_hotkey (p1now, p1pressed, m_Settings.hotkeys);
```

And after the `g_map1 = &m_Settings.map2;` boot line (~line 157), add:

```cpp
			g_hotkey_hold_mask = hotkey_hold_mask (m_Settings.hotkeys);
```

(`kernel.cpp` already includes `input/hotkey.h` and `libretro/callbacks.h`.)

- [ ] **Step 6: Run the host test + build**

Run: `make -C test test_hotkey && ./test/test_hotkey && make`
Expected: `All hotkey tests passed`; kernel builds to completion.

- [ ] **Step 7: Commit**

```bash
git add src/input/hotkey.h src/input/hotkey.cpp test/test_hotkey.cpp src/kernel.cpp
git commit -m "Input: generalize decode_hotkey to bindings + hold-mask/conflicts helpers"
```

---

### Task 4: `HotkeyScreen` remap UI

**Files:**
- Create: `src/menu/hotkey_screen.h`, `src/menu/hotkey_screen.cpp`
- Modify: `Makefile` (`OBJS`: add `src/menu/hotkey_screen.o`)

**Interfaces:**
- Consumes: `Settings`/`HotkeyBindings`/`HK_*`, `SettingsStore`, `hotkey_hold_mask`/`hotkey_conflicts`, `g_hotkey_hold_mask`, `TextCanvas`, `Gamepad`.
- Produces: `class HotkeyScreen { HotkeyScreen(TextCanvas*, Gamepad*, CUSBHCIDevice*, Settings*, SettingsStore*); void Run(void); };`.

- [ ] **Step 1: Write `src/menu/hotkey_screen.h`**

```cpp
//
// src/menu/hotkey_screen.h
//
// Bare Metal Sega Genesis
// In-game hotkey remap screen: per action, cycle the hold + trigger buttons.
// Reached from the Settings screen. Mirrors ControlsScreen.
//

#ifndef _menu_hotkey_screen_h
#define _menu_hotkey_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"

class HotkeyScreen
{
public:
    HotkeyScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                 Settings *pSettings, SettingsStore *pStore);
    void Run(void);   // returns when the user backs out (B)

private:
    void Render(int selected);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
};

#endif
```

- [ ] **Step 2: Write `src/menu/hotkey_screen.cpp`**

```cpp
//
// src/menu/hotkey_screen.cpp
//
// Bare Metal Sega Genesis
// See hotkey_screen.h.
//

#include "hotkey_screen.h"
#include "../input/joypad_map.h"          // GP_* for nav
#include "../input/hotkey.h"              // hotkey_hold_mask, hotkey_conflicts
#include "../libretro/callbacks.h"        // g_hotkey_hold_mask
#include <circle/timer.h>

#define NUM_ACT  HK_COUNT                 // 6 actions
#define NUM_ROWS (HK_COUNT * 2)           // hold + key per action

static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 RED   = 0xF800;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

static const char *const ACT_LABEL[NUM_ACT] =
    { "Quick-save", "Quick-load", "Vol +", "Vol -", "HUD", "Mute" };

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

HotkeyScreen::HotkeyScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                           CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                           SettingsStore *pStore)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore)
{
}

void HotkeyScreen::Render(int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2;
    int boxW = cw * 36, boxH = ch * (NUM_ROWS + 4);

    unsigned conflicts =
        hotkey_conflicts(m_pSettings->hotkeys, m_pSettings->menu_hotkey);

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "HOTKEYS", WHITE, BOX);

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  act   = i / 2;
        bool isKey = (i & 1);
        int  ty    = boxY + ch * (i + 3);
        bool sel   = (i == selected);
        bool bad   = (conflicts >> act) & 1u;
        u16  fg    = sel ? SELFG : (bad ? RED : WHITE);
        u16  bg    = sel ? SELBG : BOX;
        if (sel) m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        m_pCanvas->DrawText(boxX + cw, ty, sel ? ">" : " ", fg, bg);

        const HotkeyBinding &b = m_pSettings->hotkeys.b[act];
        char label[24];
        int  k = 0;
        const char *a = ACT_LABEL[act];
        for (int j = 0; a[j] && k < 16; j++) label[k++] = a[j];
        label[k++] = ' ';
        const char *f = isKey ? "key" : "mod";
        for (int j = 0; f[j] && k < 22; j++) label[k++] = f[j];
        label[k] = '\0';

        char val[12];
        val[0] = '<'; val[1] = ' ';
        const char *pl = phys_label(isKey ? b.trigger : b.hold);
        k = 2;
        for (int j = 0; pl[j] && k < 9; j++) val[k++] = pl[j];
        val[k++] = ' '; val[k++] = '>'; val[k] = '\0';

        m_pCanvas->DrawText(boxX + cw * 3,  ty, label, fg, bg);
        m_pCanvas->DrawText(boxX + cw * 18, ty, val,   fg, bg);
        if (bad) m_pCanvas->DrawText(boxX + cw * 25, ty, "!", fg, bg);
    }

    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 3),
                        "Left/Right: change   B: back", WHITE, BOX);
}

void HotkeyScreen::Run(void)
{
    int selected = 0;
    Render(selected);

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
            Render(selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = (selected + 1) % NUM_ROWS;
            Render(selected);
        }

        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            int  act   = selected / 2;
            bool isKey = (selected & 1);
            HotkeyBinding &b = m_pSettings->hotkeys.b[act];

            if (isKey)
            {
                // Cycle trigger over all 8 PadButtons, skipping the hold value.
                int v = (int) b.trigger;
                do { v = (v + dir + 8) % 8; } while (v == (int) b.hold);
                b.trigger = (PadButton) v;
            }
            else
            {
                // Cycle hold over the safe set L,R,Start,Select (values 4..7).
                int v = (int) b.hold;
                v = ((v - 4 + dir + 4) % 4) + 4;
                b.hold = (PadButton) v;
                if (b.hold == b.trigger)                 // keep hold != trigger
                    b.trigger = (PadButton) (((int) b.trigger + 1) % 8);
            }

            m_pStore->Save(*m_pSettings);
            g_hotkey_hold_mask = hotkey_hold_mask(m_pSettings->hotkeys);
            Render(selected);
        }

        if (pressed & GP_B)
        {
            return;
        }

        CTimer::SimpleMsDelay(16);
    }
}
```

- [ ] **Step 3: Add `src/menu/hotkey_screen.o` to `OBJS`**

In the root `Makefile`, after `src/menu/controls_screen.o`, add a continuation:

```make
       src/menu/controls_screen.o \
       src/menu/hotkey_screen.o \
```

(Keep the existing trailing ` \` chain intact.)

- [ ] **Step 4: Build to verify it compiles**

Run: `make`
Expected: builds to completion; `src/menu/hotkey_screen.o` compiles (the class is unused/unreached for now, which links fine).

- [ ] **Step 5: Commit**

```bash
git add src/menu/hotkey_screen.h src/menu/hotkey_screen.cpp Makefile
git commit -m "Menu: add HotkeyScreen remap UI (cycle hold/key, conflict flag)"
```

---

### Task 5: Settings screen `Hotkeys...` row + kernel wiring

**Files:**
- Modify: `src/menu/settings_screen.h` (include, ctor param, member)
- Modify: `src/menu/settings_screen.cpp` (NUM_ROWS, labels/values, START handler)
- Modify: `src/kernel.h` (member), `src/kernel.cpp` (construct + pass)

**Interfaces:**
- Consumes: `HotkeyScreen` (Task 4).
- Produces: a `Hotkeys...` nav row (index 12) opening `HotkeyScreen`.

- [ ] **Step 1: Add the `HotkeyScreen*` dependency to `SettingsScreen`**

In `src/menu/settings_screen.h`, add the include:

```cpp
#include "../ui/overlay.h"
#include "hotkey_screen.h"
```

Extend the constructor (add a trailing `HotkeyScreen *pHotkey`):

```cpp
                   VideoModeScreen *pVideoMode, Overlay *pOverlay,
                   HotkeyScreen *pHotkey);
```

Add the member (after `m_pOverlay`):

```cpp
    Overlay         *m_pOverlay;
    HotkeyScreen    *m_pHotkey;
```

- [ ] **Step 2: Update the constructor definition**

In `src/menu/settings_screen.cpp`, update the signature + init list:

```cpp
                               VideoModeScreen *pVideoMode, Overlay *pOverlay,
                               HotkeyScreen *pHotkey)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay),
    m_pAudio(pAudio), m_pRomPath(0), m_pControls(pControls),
    m_pVideoMode(pVideoMode), m_pOverlay(pOverlay), m_pHotkey(pHotkey)
{
}
```

- [ ] **Step 3: Add the `Hotkeys...` row**

In `src/menu/settings_screen.cpp` change (line 16):

```cpp
#define NUM_ROWS 13
```

Append `"Hotkeys..."` to `labels` (after `"Video Mode..."`) and a `""` to `values`:

```cpp
                                     "Vsync:", "Debug Overlay:",
                                     "Controls...", "Video Mode...",
                                     "Hotkeys..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, audioVal,
                                     vsyncVal, dbgVal, "", "", "" };
```

- [ ] **Step 4: Handle the new nav row in the `GP_START` block**

After the `else if (selected == 11)` (Video Mode) block, add:

```cpp
            else if (selected == 12)                  // Hotkeys...
            {
                m_pHotkey->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
```

- [ ] **Step 5: Add the kernel member + construct + pass**

In `src/kernel.h`, after `ControlsScreen m_ControlsScreen;` add:

```cpp
	ControlsScreen     m_ControlsScreen; // button-remapping sub-screen
	HotkeyScreen       m_HotkeyScreen;   // in-game hotkey remap sub-screen
```

Add the include near the other menu includes (where `controls_screen.h` is included — check `kernel.h` includes; add `#include "menu/hotkey_screen.h"`).

In `src/kernel.cpp` init list, construct it before `m_SettingsScreen` (after `m_ControlsScreen (...)`):

```cpp
	m_ControlsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore),
	m_HotkeyScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore),
```

And add `&m_HotkeyScreen` as the trailing argument to the `m_SettingsScreen (...)` constructor call:

```cpp
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display, &m_Audio, &m_ControlsScreen, &m_VideoModeScreen, &m_Overlay, &m_HotkeyScreen),
```

- [ ] **Step 6: Build + run the full host suite**

Run: `make && make -C test run`
Expected: kernel builds; all host suites pass (`All hotkey tests passed`, `All settings tests passed`, etc.).

- [ ] **Step 7: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp src/kernel.h src/kernel.cpp
git commit -m "Settings: add Hotkeys... row opening HotkeyScreen; wire into kernel"
```

---

### Task 6: Hardware-verify checklist (section O)

**Files:**
- Modify: `docs/hardware-verification-checklist-2026-06-20.md`

**Interfaces:** none (documentation).

- [ ] **Step 1: Append section O and a summary row**

After the `## N. In-game hotkeys + toasts` section (before `## Results summary`), add:

```markdown
## O. Hotkey remapping

- [ ] **O1 — Remap works.** Settings → `Hotkeys...`; change an action's key (and/or
  hold). The new combo performs the action in-game; the old combo no longer does.
- [ ] **O2 — Persisted.** The `hotkey_*` keys appear in `SD:/settings.txt` and the
  remap survives reboot.
- [ ] **O3 — Conflict flag.** Set two actions to the same combo: both rows show a
  red `!`; the higher-priority action (earlier in the list) still works.
- [ ] **O4 — Suppression follows hold.** Change an action's hold to L; while L is
  held, player-1 input is masked from the game. Select-held still masks if any
  action uses Select.
- [ ] **O5 — Defaults.** With a fresh `settings.txt`: Quick-save=Select+X,
  load=Select+Y, HUD=Select+A, mute=Select+B, vol+=Select+L, vol-=Select+R.
- [ ] **O6 — No menu collision.** The configured `menu_hotkey` still opens the
  pause menu.
```

In the Results summary table, add after the `| N. Hotkeys + toasts | | |` row:

```markdown
| O. Hotkey remapping | | |
```

- [ ] **Step 2: Commit**

```bash
git add docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Docs: add hotkey remapping hardware-verify checklist (section O)"
```

---

## Self-Review Notes

- **Spec coverage:** binding model + serialize/parse + defaults → Task 1; suppression global → Task 2; generalized decoder + hold-mask + conflicts → Task 3; remap UI → Task 4; Settings row + kernel wiring → Task 5; checklist O → Task 6. All spec sections mapped.
- **Build stays green each task:** the `decode_hotkey` signature change (Task 3) updates its test AND the kernel call in the same task; the suppression global (Task 2) lands before it's referenced (Task 3 boot mask, Task 4 screen).
- **Type consistency:** `HotkeyBindings`/`HotkeyBinding`/`HK_*` (Task 1) used identically in Tasks 3, 4; `decode_hotkey(unsigned,unsigned,const HotkeyBindings&)`, `hotkey_hold_mask`, `hotkey_conflicts` (Task 3) used in Tasks 4-5; `g_hotkey_hold_mask` (Task 2) used in Tasks 3, 4.
- **Index map:** `HK_*` index `i` → `InGameAction (i+1)`, valid because `InGameAction` is `{None, QuickSave, QuickLoad, VolUp, VolDown, ToggleHud, Mute}` in that order.
- **Safe-hold contiguity:** PadButton `{A,B,X,Y,L,R,Start,Select}` makes the safe holds values 4..7, used by both the parse validator (Task 1) and the hold cycle (Task 4).
- **No Blit/Display/vsync changes.**
