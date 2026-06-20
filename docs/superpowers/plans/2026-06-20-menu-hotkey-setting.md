# Menu Hotkey Setting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the in-emulation menu hotkey user-configurable from a preset list, replacing the hardcoded Start+Select.

**Architecture:** A `MenuHotkey` enum + `menu_hotkey` field + keyword parse/serialize live in the pure settings module; the enum→`GP_*` bitmask mapping (`hotkey_mask`) lives in `joypad_map` (where the `GP_*` defines and the libretro include path already are); a 7th Settings-screen row cycles the presets; the kernel computes the mask per frame from live settings.

**Tech Stack:** C++ (bare-metal, Circle), Genesis-Plus-GX-Wide libretro core, host-side g++ unit tests.

## Naming note (refinement from spec)

The spec placed `hotkey_mask` in `settings.cpp`. Because `joypad_map.h` (which
owns the `GP_*` bit defines) includes `<libretro.h>`, putting `hotkey_mask` there
would drag the libretro header into the otherwise-pure `settings` module and its
host-test rule. Instead `hotkey_mask` lives in `joypad_map.{h,cpp}` (already has
the `GP_*` defines and the libretro include path in its test rule); `joypad_map.h`
includes the pure `settings.h` for the `MenuHotkey` enum. Behavior is identical.

## Global Constraints

- `src/settings/settings.{h,cpp}` stays free of `<libretro.h>` / Circle (host-testable with the plain `test_settings` rule, which has no `-I` for libretro).
- Presets: `StartSelect` (default), `StartA`, `StartB`, `LR`. File keywords: `start+select` / `start+a` / `start+b` / `l+r`.
- Masks: `StartSelect → GP_START|GP_SELECT`, `StartA → GP_START|GP_A`, `StartB → GP_START|GP_B`, `LR → GP_LB|GP_RB`.
- Unknown `menu_hotkey` value parses to `StartSelect`.
- Build `-Wall -Wextra`; member-init order matches declaration order.

---

## File Structure

**Modified files only:**
- `src/settings/settings.h` / `.cpp` — `MenuHotkey` enum, `menu_hotkey` field, `menu_hotkey_file_value`, parse/serialize.
- `src/input/joypad_map.h` / `.cpp` — `hotkey_mask(MenuHotkey)`; include `settings.h`.
- `test/test_settings.cpp` — keyword parse/round-trip/file-value.
- `test/test_joypad.cpp` — `hotkey_mask` mappings.
- `src/menu/settings_screen.h`-unchanged / `.cpp` — 7th row (Menu Hotkey).
- `src/kernel.cpp` — per-frame mask from `m_Settings`.

No Makefile changes (no new files; `joypad_map.cpp` and `settings.cpp` already built).

---

## Task 1: Settings model — MenuHotkey enum + keyword parse/serialize

**Files:**
- Modify: `src/settings/settings.h`
- Modify: `src/settings/settings.cpp`
- Modify: `test/test_settings.cpp`

**Interfaces:**
- Produces:
  - `enum class MenuHotkey { StartSelect, StartA, StartB, LR };`
  - `Settings` gains `MenuHotkey menu_hotkey;` (default `StartSelect`).
  - `const char *menu_hotkey_file_value(MenuHotkey h);` → `"start+select"`/`"start+a"`/`"start+b"`/`"l+r"`.
  - File key `menu_hotkey`.

- [ ] **Step 1: Write the failing test additions**

In `test/test_settings.cpp`, add before the `printf("All settings tests passed\n");` line:

```cpp
    // Menu hotkey defaults + parse + invalid fallback.
    assert(d.menu_hotkey == MenuHotkey::StartSelect);
    assert(parse_settings("menu_hotkey=start+a\n").menu_hotkey == MenuHotkey::StartA);
    assert(parse_settings("menu_hotkey=start+b\n").menu_hotkey == MenuHotkey::StartB);
    assert(parse_settings("menu_hotkey=l+r\n").menu_hotkey      == MenuHotkey::LR);
    assert(parse_settings("menu_hotkey=bogus\n").menu_hotkey    == MenuHotkey::StartSelect);

    // File-value mapping.
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::StartSelect), "start+select") == 0);
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::StartA),      "start+a")      == 0);
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::StartB),      "start+b")      == 0);
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::LR),          "l+r")          == 0);

    // Round-trip.
    Settings hs; hs.menu_hotkey = MenuHotkey::LR;
    char hbuf[512];
    serialize_settings(hs, hbuf, sizeof hbuf);
    assert(parse_settings(hbuf).menu_hotkey == MenuHotkey::LR);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_settings`
Expected: FAIL — `'MenuHotkey' has not been declared` / no member `menu_hotkey`.

- [ ] **Step 3: Add the enum, field, and declaration to `settings.h`**

In `src/settings/settings.h`, replace:

```cpp
enum class ScaleMode { Integer, Stretch };
enum class Region    { Auto, NTSC, PAL };
```

with:

```cpp
enum class ScaleMode { Integer, Stretch };
enum class Region    { Auto, NTSC, PAL };
enum class MenuHotkey { StartSelect, StartA, StartB, LR };
```

Add the field to `Settings` (after `auto_launch_rom`) and init it. Replace the
struct's field block + constructor:

```cpp
    Region    region;       // region: auto | ntsc | pal
    char      auto_launch_rom[256];   // ROM path to boot into ("" = unset)

    Settings(void)
    :   scale_mode(ScaleMode::Integer), widescreen(false),
        volume(100), mute(false), region(Region::Auto)
    {
        auto_launch_rom[0] = '\0';
    }
```

with:

```cpp
    Region    region;       // region: auto | ntsc | pal
    char      auto_launch_rom[256];   // ROM path to boot into ("" = unset)
    MenuHotkey menu_hotkey; // controller combo that opens the pause menu

    Settings(void)
    :   scale_mode(ScaleMode::Integer), widescreen(false),
        volume(100), mute(false), region(Region::Auto),
        menu_hotkey(MenuHotkey::StartSelect)
    {
        auto_launch_rom[0] = '\0';
    }
```

Then, after the `region_core_value` declaration, add:

```cpp
// Menu hotkey as written to the settings file
// ("start+select" | "start+a" | "start+b" | "l+r").
const char *menu_hotkey_file_value(MenuHotkey h);
```

- [ ] **Step 4: Parse the new key in `settings.cpp`**

In `src/settings/settings.cpp`, in `parse_settings`, replace the
`auto_launch_rom` clause's trailing `// unknown keys: ignored`:

```cpp
        else if (ieq(key, "auto_launch_rom"))
        {
            unsigned i = 0;
            for (; val[i] && i < sizeof(s.auto_launch_rom) - 1; i++)
                s.auto_launch_rom[i] = val[i];
            s.auto_launch_rom[i] = '\0';
        }
        // unknown keys: ignored
```

with:

```cpp
        else if (ieq(key, "auto_launch_rom"))
        {
            unsigned i = 0;
            for (; val[i] && i < sizeof(s.auto_launch_rom) - 1; i++)
                s.auto_launch_rom[i] = val[i];
            s.auto_launch_rom[i] = '\0';
        }
        else if (ieq(key, "menu_hotkey"))
        {
            if      (ieq(val, "start+a")) s.menu_hotkey = MenuHotkey::StartA;
            else if (ieq(val, "start+b")) s.menu_hotkey = MenuHotkey::StartB;
            else if (ieq(val, "l+r"))     s.menu_hotkey = MenuHotkey::LR;
            else                          s.menu_hotkey = MenuHotkey::StartSelect;
        }
        // unknown keys: ignored
```

- [ ] **Step 5: Add the file-value helper and serialize the new key**

In `src/settings/settings.cpp`, add the helper just above `serialize_settings`
(near `region_file_value`):

```cpp
const char *menu_hotkey_file_value(MenuHotkey h)
{
    switch (h)
    {
    case MenuHotkey::StartA: return "start+a";
    case MenuHotkey::StartB: return "start+b";
    case MenuHotkey::LR:     return "l+r";
    default:                 return "start+select";
    }
}
```

Then in `serialize_settings`, replace the auto_launch_rom tail:

```cpp
    appendz(out, out_size, "\nauto_launch_rom=");
    appendz(out, out_size, s.auto_launch_rom);
    appendz(out, out_size, "\n");
```

with:

```cpp
    appendz(out, out_size, "\nauto_launch_rom=");
    appendz(out, out_size, s.auto_launch_rom);
    appendz(out, out_size, "\nmenu_hotkey=");
    appendz(out, out_size, menu_hotkey_file_value(s.menu_hotkey));
    appendz(out, out_size, "\n");
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: `All settings tests passed`

- [ ] **Step 7: Build the kernel to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 8: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add menu_hotkey field + keyword parse/serialize"
```

---

## Task 2: hotkey_mask mapping in joypad_map

**Files:**
- Modify: `src/input/joypad_map.h`
- Modify: `src/input/joypad_map.cpp`
- Modify: `test/test_joypad.cpp`

**Interfaces:**
- Consumes: `MenuHotkey` (Task 1); `GP_*` defines (this file).
- Produces: `unsigned hotkey_mask(MenuHotkey h);` → the `GP_*` button bitmask.

- [ ] **Step 1: Write the failing test additions**

In `test/test_joypad.cpp`, add before the `printf("All joypad tests passed\n");` line:

```cpp
    // Menu-hotkey preset -> button bitmask.
    assert(hotkey_mask(MenuHotkey::StartSelect) == (GP_START | GP_SELECT));
    assert(hotkey_mask(MenuHotkey::StartA)      == (GP_START | GP_A));
    assert(hotkey_mask(MenuHotkey::StartB)      == (GP_START | GP_B));
    assert(hotkey_mask(MenuHotkey::LR)          == (GP_LB | GP_RB));
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_joypad`
Expected: FAIL — `'hotkey_mask' was not declared` / `'MenuHotkey' has not been declared`.

- [ ] **Step 3: Declare `hotkey_mask` in `joypad_map.h`**

In `src/input/joypad_map.h`, add the settings include after the existing
includes (after `#include <libretro.h>`):

```cpp
#include "../settings/settings.h"   // MenuHotkey
```

Before the closing `#endif`, add:

```cpp
// The GP_* button bitmask for a menu-hotkey preset (both buttons held opens
// the pause menu).
unsigned hotkey_mask(MenuHotkey h);
```

- [ ] **Step 4: Implement `hotkey_mask` in `joypad_map.cpp`**

In `src/input/joypad_map.cpp`, append at the end of the file:

```cpp
unsigned hotkey_mask(MenuHotkey h)
{
    switch (h)
    {
    case MenuHotkey::StartA: return GP_START | GP_A;
    case MenuHotkey::StartB: return GP_START | GP_B;
    case MenuHotkey::LR:     return GP_LB | GP_RB;
    default:                 return GP_START | GP_SELECT;
    }
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd test && make test_joypad && ./test_joypad`
Expected: `All joypad tests passed`

- [ ] **Step 6: Build the kernel to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 7: Commit**

```bash
git add src/input/joypad_map.h src/input/joypad_map.cpp test/test_joypad.cpp
git commit -m "Input: hotkey_mask maps MenuHotkey presets to GP_* bitmasks"
```

---

## Task 3: Settings screen — Menu Hotkey row

**Files:**
- Modify: `src/menu/settings_screen.cpp`

**Interfaces:**
- Consumes: `Settings.menu_hotkey` (Task 1).

- [ ] **Step 1: Grow to 7 rows**

In `src/menu/settings_screen.cpp`, change `#define NUM_ROWS 6` to:

```cpp
#define NUM_ROWS 7
```

- [ ] **Step 2: Add the Menu Hotkey label + value to Render**

In `src/menu/settings_screen.cpp`, replace the label/value setup block:

```cpp
    bool autoOn = m_pRomPath != 0 &&
                  strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
    const char *autoVal = autoOn ? "< On >" : "< Off >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal };
```

with:

```cpp
    bool autoOn = m_pRomPath != 0 &&
                  strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
    const char *autoVal = autoOn ? "< On >" : "< Off >";
    const char *hotkeyVal =
        m_pSettings->menu_hotkey == MenuHotkey::StartA ? "< Start+A >" :
        m_pSettings->menu_hotkey == MenuHotkey::StartB ? "< Start+B >" :
        m_pSettings->menu_hotkey == MenuHotkey::LR     ? "< L+R >"     :
                                                         "< Start+Select >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal };
```

- [ ] **Step 3: Handle the Menu Hotkey edit in Run()**

In `src/menu/settings_screen.cpp`, in `Run`, replace the auto-launch `case 5`
block's closing (the `case 5: { ... } }` that ends the switch) — specifically
replace:

```cpp
            case 5:   // Auto-launch this game (toggle)
            {
                bool on = m_pRomPath != 0 &&
                          strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
                if (on)
                {
                    m_pSettings->auto_launch_rom[0] = '\0';
                }
                else if (m_pRomPath != 0)
                {
                    strncpy(m_pSettings->auto_launch_rom, m_pRomPath,
                            sizeof(m_pSettings->auto_launch_rom) - 1);
                    m_pSettings->auto_launch_rom[
                        sizeof(m_pSettings->auto_launch_rom) - 1] = '\0';
                }
                break;
            }
            }
```

with:

```cpp
            case 5:   // Auto-launch this game (toggle)
            {
                bool on = m_pRomPath != 0 &&
                          strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
                if (on)
                {
                    m_pSettings->auto_launch_rom[0] = '\0';
                }
                else if (m_pRomPath != 0)
                {
                    strncpy(m_pSettings->auto_launch_rom, m_pRomPath,
                            sizeof(m_pSettings->auto_launch_rom) - 1);
                    m_pSettings->auto_launch_rom[
                        sizeof(m_pSettings->auto_launch_rom) - 1] = '\0';
                }
                break;
            }
            case 6:   // Menu Hotkey (cycle the 4 presets)
            {
                int h = (int) m_pSettings->menu_hotkey + dir;
                if (h < 0) h = 3;
                if (h > 3) h = 0;
                m_pSettings->menu_hotkey = (MenuHotkey) h;
                break;
            }
            }
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 5: Commit**

```bash
git add src/menu/settings_screen.cpp
git commit -m "Settings: Menu Hotkey row in the Settings screen"
```

---

## Task 4: Kernel — use the configured hotkey

**Files:**
- Modify: `src/kernel.cpp`

**Interfaces:**
- Consumes: `hotkey_mask` (Task 2); `Settings.menu_hotkey` (Task 1).

- [ ] **Step 1: Remove the hardcoded HOTKEY constant**

In `src/kernel.cpp`, delete the line:

```cpp
	const unsigned HOTKEY = GP_START | GP_SELECT;
```

- [ ] **Step 2: Compute the mask per frame and use it**

In `src/kernel.cpp`, in the play loop, replace:

```cpp
			unsigned now     = m_Gamepad.MenuButtons ();
			unsigned pressed = now & ~prevBtns;
			prevBtns = now;

			// Hotkey: Start+Select both held, completed this frame.
			if ((pressed & HOTKEY) && (now & HOTKEY) == HOTKEY)
```

with:

```cpp
			unsigned now     = m_Gamepad.MenuButtons ();
			unsigned pressed = now & ~prevBtns;
			prevBtns = now;

			// Configurable menu hotkey (both buttons held), read live.
			unsigned hotkey = hotkey_mask (m_Settings.menu_hotkey);
			if ((pressed & hotkey) && (now & hotkey) == hotkey)
```

(`hotkey_mask` is declared in `src/input/joypad_map.h`, already included by
`kernel.cpp`; `m_Settings` is the kernel's settings member.)

- [ ] **Step 3: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors or unused-variable warnings.

- [ ] **Step 4: Run the full host test suite**

Run: `cd test && make run`
Expected: all suites pass, including `All settings tests passed` and
`All joypad tests passed`.

- [ ] **Step 5: Commit**

```bash
git add src/kernel.cpp
git commit -m "Kernel: open the pause menu with the configured menu_hotkey"
```

---

## Hardware Verification (manual, after Task 4)

Not a code task — perform on the Pi:

- [ ] In a game, open Settings → set Menu Hotkey = L+R → resume; confirm L+R opens the pause menu and Start+Select no longer does.
- [ ] Set it back to Start+Select; confirm Start+Select opens the menu again.
- [ ] Power-cycle → confirm the choice persisted in `SD:/settings.txt`.

---

## Self-Review Notes

- **Spec coverage:** `MenuHotkey` enum + `menu_hotkey` field + keyword parse/serialize + default (Task 1); `hotkey_mask` preset→`GP_*` mapping (Task 2); Settings-screen row cycling the four presets (Task 3); kernel reads the mask live per frame, no reset (Task 4); host tests for keyword parse/round-trip/file-value (Task 1) and mask mappings (Task 2); hardware checklist. Press-to-bind and single/three-button hotkeys are out of scope per the spec — no task.
- **Refinement:** `hotkey_mask` lives in `joypad_map` (not `settings.cpp`) to keep `<libretro.h>` out of the pure settings module — documented in the Naming note; behavior identical.
- **Type consistency:** `MenuHotkey`/`menu_hotkey`/`menu_hotkey_file_value` defined in Task 1 used in Tasks 2–4; `hotkey_mask` defined in Task 2 used in Task 4.
- **Green builds:** each task ends with a clean full `make`; Task 3 builds standalone (kernel still uses the old constant until Task 4 — wait: Task 1 removes nothing from the kernel; the kernel's `HOTKEY` constant remains valid through Tasks 1–3 and is replaced in Task 4, so every task compiles).
