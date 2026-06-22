# 3/6-Button Pad Type Toggle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a global `pad_type` setting (3-button / 6-button, default 6) that selects the emulated Genesis controller device on ROM load.

**Architecture:** A `PadType` enum + keyword helper in the pure settings module (host-tested); the kernel picks `MDPAD_3B` (subclass 0) vs `MDPAD_6B` (subclass 1) from the setting at the existing per-ROM-load device set; one new Settings-screen toggle row.

**Tech Stack:** C++ (bare-metal, Circle), GNU make. Host unit tests in `test/`. Kernel cross-compiles to `kernel7.img` via repo-root `make`.

## Global Constraints

- Pure settings code (`src/settings/settings.cpp`) has **no Circle dependency** — it's compiled by `test/test_settings.cpp`.
- Default `pad_type = SixButton` (current behavior unchanged); unknown/missing file value → `SixButton`.
- File keyword/values: `pad_type = 3button | 6button`.
- Global (one setting, both ports); applies on **ROM reload** (no live re-set); no `Apply()` hook — the kernel reads `m_Settings.pad_type` at load.
- Device constants (from `libs/genesis-plus-gx-wide/libretro/libretro.c:59-60`): `MDPAD_3B = SUBCLASS(JOYPAD,0)`, `MDPAD_6B = SUBCLASS(JOYPAD,1)`.
- `ButtonMap` unchanged — the core gates X/Y/Z/Mode by device type.

---

### Task 1: Settings model — `PadType` (host-tested)

**Files:**
- Modify: `src/settings/settings.h` (enum near line 16; field + ctor init near the `audio_latency` member; helper declaration)
- Modify: `src/settings/settings.cpp` (parse branch after the `menu_hotkey` branch; serialize after the `menu_hotkey=` line; helper def after `menu_hotkey_file_value`)
- Test: `test/test_settings.cpp` (add a block before the final `printf`)

**Interfaces:**
- Produces:
  - `enum class PadType { ThreeButton, SixButton };`
  - `Settings::pad_type` (default `SixButton`)
  - `const char *pad_type_file_value(PadType);` → `"3button"|"6button"`
  - file key `pad_type`

- [ ] **Step 1: Write the failing tests**

In `test/test_settings.cpp`, immediately before `printf("All settings tests passed\n");`, insert:

```cpp
    // Pad type: default SixButton; parse each keyword; invalid -> SixButton.
    assert(d.pad_type == PadType::SixButton);
    assert(parse_settings("pad_type=3button\n").pad_type == PadType::ThreeButton);
    assert(parse_settings("pad_type=6button\n").pad_type == PadType::SixButton);
    assert(parse_settings("pad_type=bogus\n").pad_type    == PadType::SixButton);

    // File-value mapping.
    assert(strcmp(pad_type_file_value(PadType::ThreeButton), "3button") == 0);
    assert(strcmp(pad_type_file_value(PadType::SixButton),   "6button") == 0);

    // Round-trip.
    Settings pts; pts.pad_type = PadType::ThreeButton;
    char ptbuf[512];
    serialize_settings(pts, ptbuf, sizeof ptbuf);
    assert(parse_settings(ptbuf).pad_type == PadType::ThreeButton);
```

- [ ] **Step 2: Run the test to verify it fails (does not compile)**

Run: `cd test && make test_settings`
Expected: FAIL — `'PadType' was not declared` / `pad_type` is not a member.

- [ ] **Step 3: Add the enum, field, and ctor init in `settings.h`**

After the line `enum class AudioLatency { Low, Medium, High };` add:

```cpp
enum class PadType { ThreeButton, SixButton };
```

In `struct Settings`, after the `AudioLatency audio_latency;` line, add:

```cpp
    PadType pad_type;       // emulated Genesis pad: 3-button or 6-button
```

In the constructor initializer list, change the tail `audio_latency(AudioLatency::Medium)` to:

```cpp
        audio_latency(AudioLatency::Medium), pad_type(PadType::SixButton)
```

- [ ] **Step 4: Declare the helper in `settings.h`**

After the `audio_latency_frames` declaration, add:

```cpp
// PadType as written to the settings file ("3button" | "6button").
const char *pad_type_file_value(PadType p);
```

- [ ] **Step 5: Implement parse + serialize + helper in `settings.cpp`**

In `parse_settings`, after the `menu_hotkey` branch (the block ending
`else  s.menu_hotkey = MenuHotkey::StartSelect; }`), add:

```cpp
        else if (ieq(key, "pad_type"))
            s.pad_type = ieq(val, "3button") ? PadType::ThreeButton
                                              : PadType::SixButton;
```

After the `menu_hotkey_file_value` function definition, add:

```cpp
const char *pad_type_file_value(PadType p)
{
    return p == PadType::ThreeButton ? "3button" : "6button";
}
```

In `serialize_settings`, after the two `menu_hotkey=` append lines
(ending `menu_hotkey_file_value(s.menu_hotkey)`), add:

```cpp
    appendz(out, out_size, "\npad_type=");
    appendz(out, out_size, pad_type_file_value(s.pad_type));
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: PASS — `All settings tests passed`.

- [ ] **Step 7: Run the full host suite (no regressions)**

Run: `cd test && make && ./test_settings && ./test_joypad && ./test_splash`
Expected: `All settings tests passed`, `All joypad tests passed`, `All splash tests passed`.

- [ ] **Step 8: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add pad_type (3/6-button) field + parse/serialize

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Kernel device selection

**Files:**
- Modify: `src/kernel.cpp` (add `MDPAD_3B` define near line 192; setting-driven device set at lines 267-268)

**Interfaces:**
- Consumes: `Settings::pad_type`, `PadType` (Task 1).
- Produces: the controller port device is `MDPAD_3B` when `pad_type == ThreeButton`, else `MDPAD_6B`.

- [ ] **Step 1: Add the 3-button device constant**

In `src/kernel.cpp`, after the line
`#define RETRO_DEVICE_MDPAD_6B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)`, add:

```cpp
	#define RETRO_DEVICE_MDPAD_3B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
```

- [ ] **Step 2: Select the device from the setting**

Replace the hardcoded device set:

```cpp
		retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
		retro_set_controller_port_device (1, RETRO_DEVICE_MDPAD_6B);
```

with:

```cpp
		unsigned padDev = (m_Settings.pad_type == PadType::ThreeButton)
		                  ? RETRO_DEVICE_MDPAD_3B : RETRO_DEVICE_MDPAD_6B;
		retro_set_controller_port_device (0, padDev);
		retro_set_controller_port_device (1, padDev);
```

- [ ] **Step 3: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion, produces `kernel7.img`, no errors. (`settings.h` is already included via `kernel.h`, so `PadType` is visible.)

- [ ] **Step 4: Commit**

```bash
git add src/kernel.cpp
git commit -m "Kernel: select MDPAD_3B/6B device from the pad_type setting on ROM load

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Settings screen row + hardware checklist

**Files:**
- Modify: `src/menu/settings_screen.cpp` (NUM_ROWS line 16; `padVal` + labels/values arrays ~lines 99-110; new `case 11` in the dir switch ~line 239; GP_START action indices ~lines 248/254/260; footer ~line 127)
- Modify: `docs/hardware-verification-checklist-2026-06-20.md` (append a section)

**Interfaces:**
- Consumes: `Settings::pad_type`, `PadType` (Task 1).
- Produces: a `Pad Type: < 6-button | 3-button >` cycle row at index 11; action rows shift to Controls=12, Video Mode=13, Hotkeys=14.

- [ ] **Step 1: Bump the row count**

In `src/menu/settings_screen.cpp`, change:

```cpp
#define NUM_ROWS 14
```

to:

```cpp
#define NUM_ROWS 15
```

- [ ] **Step 2: Add the display value**

In `Render`, after the `latVal` definition (the 3-line `audio_latency` ternary ending `"< Medium >";`), add:

```cpp
    const char *padVal = m_pSettings->pad_type == PadType::ThreeButton
                             ? "< 3-button >" : "< 6-button >";
```

- [ ] **Step 3: Insert the label and value into the arrays**

Change the `labels[NUM_ROWS]` and `values[NUM_ROWS]` initializers so `"Pad Type:"`/`padVal` sit between `"Audio Latency:"`/`latVal` and `"Controls..."`:

```cpp
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Audio out:",
                                     "Vsync:", "Debug Overlay:",
                                     "Audio Latency:", "Pad Type:",
                                     "Controls...", "Video Mode...",
                                     "Hotkeys..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, audioVal,
                                     vsyncVal, dbgVal, latVal, padVal,
                                     "", "", "" };
```

- [ ] **Step 4: Add the toggle handler**

In the `dir != 0` switch in `Run`, after `case 10:` (Audio Latency, which ends with its `break;` then the closing `}` of that case), add a `case 11` before the switch's closing `}`:

```cpp
            case 11:  // Pad Type (toggle 3/6-button; applies on ROM reload)
                m_pSettings->pad_type =
                    m_pSettings->pad_type == PadType::SixButton
                        ? PadType::ThreeButton : PadType::SixButton;
                break;
```

- [ ] **Step 5: Renumber the action rows**

In the `pressed & GP_START` block, update the three indices (each +1):

```cpp
            if (selected == 12)                       // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 13)                  // Video Mode...
            {
                m_pVideoMode->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 14)                  // Hotkeys...
            {
                m_pHotkey->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
```

- [ ] **Step 6: Note the reload requirement in the footer**

Change the footer hint line:

```cpp
                        "Widescreen: reset.  Region: reload.  Audio: reboot.",
```

to:

```cpp
                        "Widescreen: reset.  Region/Pad: reload.  Audio: reboot.",
```

- [ ] **Step 7: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion, `kernel7.img` produced, no errors.

- [ ] **Step 8: Append the hardware-verify checklist section**

In `docs/hardware-verification-checklist-2026-06-20.md`, before the `## Results summary` section, add:

```markdown
## R. Pad type (3/6-button)

- [ ] **R1 — Switch to 3-button.** Pause → Settings → **Pad Type** = 3-button,
  reload the game (browser relaunch or power-cycle). **Expect:** the game now sees
  a 3-button pad; X/Y/Z/Mode no longer register. A title that misbehaved with a
  6-button pad now plays correctly.
- [ ] **R2 — Back to 6-button.** Set Pad Type = 6-button, reload. **Expect:** the
  X/Y/Z/Mode buttons work again.
- [ ] **R3 — Persisted.** Confirm `pad_type` in `SD:/settings.txt`; survives reboot.
```

Also add a row to the results-summary table (after the last row):

```markdown
| R. Pad type | | |
```

- [ ] **Step 9: Commit**

```bash
git add src/menu/settings_screen.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Settings UI: Pad Type row (3/6-button) + hardware checklist

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes for the implementer

- Host tests build natively in `test/`: `cd test && make`.
- The kernel build (`make` at repo root) is the only verification for `kernel.cpp`
  and `settings_screen.cpp` — no host test covers them, consistent with the project.
- Do not touch the `ButtonMap` or `input_state_cb`; the core gates the 6-button
  inputs by device type.
</content>
