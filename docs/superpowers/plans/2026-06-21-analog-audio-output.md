# Analog (3.5mm) Audio Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the user route emulator audio to the Raspberry Pi 2's 3.5mm headphone jack instead of HDMI, chosen in the Settings screen, persisted to `SD:/settings.txt`, and applied on the next boot.

**Architecture:** Mirror the existing `region` setting end-to-end — a new `AudioOutput` enum in the host-testable settings model (parse/serialize), `AudioDriver::Initialize()` picks `CHDMISoundBaseDevice` vs `CPWMSoundBaseDevice` (both share the `CSoundBaseDevice` queue API), and a new "Audio out:" cycle row in the Settings screen. Volume/mute already live in `AudioDriver::Write()` and are device-agnostic, so they carry over unchanged.

**Tech Stack:** C++ (bare-metal, Circle framework), arm-none-eabi cross toolchain for the kernel; system `c++` for host unit tests.

## Global Constraints

- Target board: **Pi 2** (`RASPPI=2` in the Makefile). The 3.5mm jack device is `CPWMSoundBaseDevice` (`circle/sound/pwmsoundbasedevice.h`).
- Default audio output is **HDMI** — existing setups must be unaffected (no regression).
- Switching applies **on reboot**, not live — the sound device is created once at startup.
- Settings model (`src/settings/settings.{h,cpp}`) has **no Circle dependencies** and stays host-testable.
- Unknown/missing setting values fall back to the default (HDMI), matching `region`/`video_mode`.
- No I2S DAC, no live switching, no per-device latency config (all stay deferred).

---

### Task 1: Settings model — `audio_output` parse/serialize

Adds the `AudioOutput` enum, the `Settings::audio_output` field, parse/serialize support, and the `audio_output_file_value()` helper. Fully host-testable; extends the existing `test_settings.cpp` (the codebase tests every setting there, not in per-feature files).

**Files:**
- Modify: `src/settings/settings.h`
- Modify: `src/settings/settings.cpp`
- Test: `test/test_settings.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `enum class AudioOutput { HDMI, Analog };`
  - `Settings::audio_output` (type `AudioOutput`, default `AudioOutput::HDMI`)
  - `const char *audio_output_file_value(AudioOutput o);` → `"hdmi"` / `"analog"`
  - Settings text key `audio_output=hdmi|analog`

- [ ] **Step 1: Write the failing tests**

In `test/test_settings.cpp`, insert this block immediately before the final
`printf("All settings tests passed\n");` line:

```cpp
    // Audio output defaults + parse + invalid fallback + file value + round-trip.
    assert(d.audio_output == AudioOutput::HDMI);
    assert(parse_settings("audio_output=analog\n").audio_output == AudioOutput::Analog);
    assert(parse_settings("audio_output=hdmi\n").audio_output   == AudioOutput::HDMI);
    assert(parse_settings("audio_output=bogus\n").audio_output  == AudioOutput::HDMI);
    assert(strcmp(audio_output_file_value(AudioOutput::HDMI),   "hdmi")   == 0);
    assert(strcmp(audio_output_file_value(AudioOutput::Analog), "analog") == 0);
    Settings aos; aos.audio_output = AudioOutput::Analog;
    char aobuf[512];
    serialize_settings(aos, aobuf, sizeof aobuf);
    assert(parse_settings(aobuf).audio_output == AudioOutput::Analog);
```

(`d` is the defaults `Settings` already parsed at the top of `main`.)

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `make -C test test_settings`
Expected: FAIL — compile error, `AudioOutput` / `audio_output` / `audio_output_file_value` not declared.

- [ ] **Step 3: Add the enum, field, default, and helper declaration**

In `src/settings/settings.h`, add the enum alongside the others (after the
`VideoMode` enum on line 18):

```cpp
enum class VideoMode { Native, P1080, P720, P480 };
enum class AudioOutput { HDMI, Analog };
```

Add the field to the `Settings` struct (after the `video_mode` member):

```cpp
    VideoMode  video_mode;  // HDMI output mode
    AudioOutput audio_output;  // hdmi | analog (3.5mm jack)
```

Add the default to the constructor initializer list — change:

```cpp
        menu_hotkey(MenuHotkey::StartSelect), video_mode(VideoMode::Native)
```

to:

```cpp
        menu_hotkey(MenuHotkey::StartSelect), video_mode(VideoMode::Native),
        audio_output(AudioOutput::HDMI)
```

Declare the helper near `video_mode_file_value` (after line 88):

```cpp
// Audio output as written to the settings file ("hdmi" | "analog").
const char *audio_output_file_value(AudioOutput o);
```

- [ ] **Step 4: Add parse, serialize, and helper implementation**

In `src/settings/settings.cpp`, add a parse branch immediately after the
`video_mode` block (after line 123, before the `// unknown keys` comment):

```cpp
        else if (ieq(key, "audio_output"))
            s.audio_output = ieq(val, "analog") ? AudioOutput::Analog
                                                : AudioOutput::HDMI;
```

Add the helper implementation after `video_mode_file_value` (after line 272):

```cpp
const char *audio_output_file_value(AudioOutput o)
{
    return o == AudioOutput::Analog ? "analog" : "hdmi";
}
```

In `serialize_settings`, append the field after the `video_mode` block (after
line 299, before the final `appendz(out, out_size, "\n");`):

```cpp
    appendz(out, out_size, "\naudio_output=");
    appendz(out, out_size, audio_output_file_value(s.audio_output));
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_settings && ./test/test_settings`
Expected: PASS — prints `All settings tests passed`.

- [ ] **Step 6: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add audio_output (hdmi|analog) parse/serialize + host test

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: AudioDriver device selection + kernel wiring

Makes `AudioDriver` choose the concrete sound device from an `AudioOutput`, and passes the persisted setting at boot. Not host-testable (needs Circle/hardware); the cross build compiling cleanly is the gate, and a hardware-verification row (Task 3) confirms real output.

**Files:**
- Modify: `src/audio/audio_driver.h`
- Modify: `src/audio/audio_driver.cpp`
- Modify: `src/kernel.cpp:224`

**Interfaces:**
- Consumes: `AudioOutput` and `Settings::audio_output` from Task 1.
- Produces: `AudioDriver::Initialize(unsigned nSampleRate, AudioOutput out)` (the second parameter is new; replaces the old single-argument signature).

- [ ] **Step 1: Update the header**

In `src/audio/audio_driver.h`:

Add the includes next to the existing HDMI include (replace line 13
`#include <circle/sound/hdmisoundbasedevice.h>`):

```cpp
#include <circle/sound/soundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include "../settings/settings.h"   // AudioOutput
```

Change the `Initialize` declaration:

```cpp
    // Allocate the queue and start the sound device at nSampleRate.
    // out selects the physical output (HDMI or the 3.5mm analog jack).
    boolean Initialize(unsigned nSampleRate, AudioOutput out);
```

Change the device member type (replace `CHDMISoundBaseDevice *m_pDevice;`):

```cpp
    CSoundBaseDevice     *m_pDevice;
```

- [ ] **Step 2: Update the implementation**

In `src/audio/audio_driver.cpp`, change the `Initialize` signature and device
creation. Replace lines 34-45 (the signature down through the
`new CHDMISoundBaseDevice(...)` null check open) so the head of the function
reads:

```cpp
boolean AudioDriver::Initialize(unsigned nSampleRate, AudioOutput out)
{
    if (nSampleRate == 0)
    {
        return FALSE;
    }

    if (out == AudioOutput::Analog)
        m_pDevice = new CPWMSoundBaseDevice(m_pInterrupt, nSampleRate);
    else
        m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, nSampleRate);

    if (m_pDevice == 0)
    {
        return FALSE;
    }
```

The rest of the function (`AllocateQueue`, `SetWriteFormat`, `Start`,
`IsActive`) is unchanged and works through the `CSoundBaseDevice` base.

- [ ] **Step 3: Pass the setting from the kernel**

In `src/kernel.cpp`, line 224, change:

```cpp
		if (!audioInited && sampleRate > 0 && m_Audio.Initialize (sampleRate))
```

to:

```cpp
		if (!audioInited && sampleRate > 0 && m_Audio.Initialize (sampleRate, m_Settings.audio_output))
```

- [ ] **Step 4: Build the kernel to verify it compiles and links**

Run: `make`
Expected: SUCCESS — builds `kernel7.img` with no errors.

- [ ] **Step 5: Commit**

```bash
git add src/audio/audio_driver.h src/audio/audio_driver.cpp src/kernel.cpp
git commit -m "Audio: select HDMI or PWM (3.5mm) device from settings.audio_output

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Settings screen "Audio out:" row + hardware-check entry

Adds the user-facing cycle row so the setting is changeable in-menu, and records a hardware-verification step. Completes the feature.

**Files:**
- Modify: `src/menu/settings_screen.cpp`
- Modify: `docs/hardware-verification-checklist-2026-06-20.md`

**Interfaces:**
- Consumes: `Settings::audio_output`, `AudioOutput` (Task 1). `AudioOutput` is visible via `settings_screen.h`'s include of `settings.h` (the screen already references `Region`/`VideoMode`).
- Produces: nothing other tasks depend on.

- [ ] **Step 1: Grow the row count**

In `src/menu/settings_screen.cpp`, change line 16:

```cpp
#define NUM_ROWS 10
```

- [ ] **Step 2: Add the value string and the row**

In `Render()`, add the value string after the `hotkeyVal` block (after line 85):

```cpp
    const char *audioVal = m_pSettings->audio_output == AudioOutput::Analog
                               ? "< Analog >" : "< HDMI >";
```

Replace the `labels` and `values` arrays (lines 86-92) with — note "Audio out:"
is inserted before the two submenu rows so the cycle rows stay grouped:

```cpp
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Audio out:",
                                     "Controls...", "Video Mode..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, audioVal,
                                     "", "" };
```

- [ ] **Step 3: Add the toggle handler and fix the submenu indices**

In `Run()`, add a new `case 7` to the left/right `switch (selected)` block,
immediately after the Menu Hotkey `case 6` block (after line 195):

```cpp
            case 7:   // Audio out (toggle HDMI <-> Analog; applies on reboot)
                m_pSettings->audio_output =
                    m_pSettings->audio_output == AudioOutput::HDMI
                        ? AudioOutput::Analog : AudioOutput::HDMI;
                break;
```

The two submenu rows shifted down by one, so update the `GP_START` handler
(lines 202-216). Replace:

```cpp
        if (pressed & GP_START)
        {
            if (selected == 7)                        // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 8)                   // Video Mode...
            {
                m_pVideoMode->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
        }
```

with:

```cpp
        if (pressed & GP_START)
        {
            if (selected == 8)                        // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 9)                   // Video Mode...
            {
                m_pVideoMode->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
        }
```

- [ ] **Step 4: Update the footnote**

Replace the first footnote line (lines 106-108):

```cpp
    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 3),
                        "Widescreen: reset.  Region: reload.  Audio: reboot.",
                        WHITE, BOX);
```

- [ ] **Step 5: Build to verify it compiles**

Run: `make`
Expected: SUCCESS — builds `kernel7.img` with no errors.

- [ ] **Step 6: Add the hardware-verification entry**

In `docs/hardware-verification-checklist-2026-06-20.md`, add a new section
immediately before the `## Results summary` line:

```markdown
## J. Analog (3.5mm) audio output

- [ ] **J1 — Switch to analog.** Pause → Settings → **Audio out** = Analog.
  Plug headphones/speakers into the Pi's 3.5mm jack and **reboot** (applies on
  boot, not live). **Expect:** game audio comes out of the 3.5mm jack; HDMI is
  silent.
- [ ] **J2 — Volume/mute still work on analog.** With Analog selected, change
  volume and toggle mute. **Expect:** they scale/silence the analog output the
  same as HDMI.
- [ ] **J3 — Persisted.** Confirm `audio_output=analog` in `SD:/settings.txt`.
- [ ] **J4 — Back to HDMI.** Set Audio out = HDMI, reboot.
  **Expect:** audio returns to HDMI; `audio_output=hdmi` in the file.

---
```

Also add the row to the results-summary table (after the
`| I. Mode auto-revert | | |` row):

```markdown
| J. Analog audio | | |
```

- [ ] **Step 7: Commit**

```bash
git add src/menu/settings_screen.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Settings UI: Audio out row (HDMI/Analog) + hardware-check entry

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes / deviations from the spec

- The spec proposed a new `test/test_audio_output.cpp`; this plan instead
  **extends `test/test_settings.cpp`**, matching the established pattern (every
  setting — region, video_mode, menu_hotkey, volume — is tested there). This
  removes the spec's `test/Makefile` and `.gitignore` changes (no new binary).
- `AudioDriver::Initialize` takes a typed `AudioOutput` (from `settings.h`)
  rather than a bool. This matches how the codebase already feeds settings enums
  into drivers (`Display::SetScaleMode(ScaleMode)`).
