# Configurable Audio Latency Setting — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a 3-way `audio_latency` (low/medium/high) setting that scales how many video-frames of audio the play loop buffers, applied live, with `medium` reproducing today's exact pacing.

**Architecture:** A new `AudioLatency` enum + two pure helpers in the existing `settings` module (host-tested), a one-line live read in the kernel pacing loop that replaces the hard-coded `target = framesPerVideo * 2`, and one cycle row on the Settings screen. No Circle audio-device code changes; the device queue allocation (`QUEUE_MS = 80`) is untouched.

**Tech Stack:** C++ (bare-metal, Circle), GNU make. Host unit tests compile/run natively in `test/`. Kernel cross-compiles to `kernel7.img` via the repo-root `make`.

## Global Constraints

- Pure settings code (`src/settings/*`) has **no Circle dependencies** — it must stay host-compilable (it is included by `test/test_settings.cpp`).
- The pacing model (timer + high-watermark busy-wait gate) must **not** be replaced — only its target depth is parameterized. (See `[[project-genesis-perf-ceiling]]`.)
- Default `audio_latency = Medium` must reproduce today's numbers **exactly**: `target = framesPerVideo * 2`, gate `= target + framesPerVideo`.
- File key/values: `audio_latency = low | medium | high`. Unknown/missing → `Medium`.
- Frame multipliers: Low = 1, Medium = 2, High = 3. Gate stays `target + framesPerVideo` for every preset.
- Follow existing patterns: keyword helpers live beside `region_file_value`/`menu_hotkey_file_value` in `settings.cpp`; the Settings row mirrors the Menu Hotkey row.

---

### Task 1: Settings model + pure helpers (host-tested)

**Files:**
- Modify: `src/settings/settings.h` (enum near line 19; field + ctor init near lines 73-81; two helper declarations near lines 116-119)
- Modify: `src/settings/settings.cpp` (parse branch near line 129; serialize line near line 348; two helper definitions near line 314)
- Test: `test/test_settings.cpp` (add a block before the final `printf`, ~line 213)

**Interfaces:**
- Produces:
  - `enum class AudioLatency { Low, Medium, High };`
  - `Settings::audio_latency` (type `AudioLatency`, default `Medium`)
  - `const char *audio_latency_file_value(AudioLatency l);` → `"low"|"medium"|"high"`
  - `unsigned audio_latency_frames(AudioLatency l);` → `1|2|3`
  - settings-file key `audio_latency`

- [ ] **Step 1: Write the failing tests**

In `test/test_settings.cpp`, insert this block immediately before `printf("All settings tests passed\n");`:

```cpp
    // Audio latency: default Medium; parse each keyword; invalid -> Medium.
    assert(d.audio_latency == AudioLatency::Medium);
    assert(parse_settings("audio_latency=low\n").audio_latency    == AudioLatency::Low);
    assert(parse_settings("audio_latency=medium\n").audio_latency == AudioLatency::Medium);
    assert(parse_settings("audio_latency=high\n").audio_latency   == AudioLatency::High);
    assert(parse_settings("audio_latency=bogus\n").audio_latency  == AudioLatency::Medium);

    // Frame multipliers.
    assert(audio_latency_frames(AudioLatency::Low)    == 1);
    assert(audio_latency_frames(AudioLatency::Medium) == 2);
    assert(audio_latency_frames(AudioLatency::High)   == 3);

    // File-value mapping.
    assert(strcmp(audio_latency_file_value(AudioLatency::Low),    "low")    == 0);
    assert(strcmp(audio_latency_file_value(AudioLatency::Medium), "medium") == 0);
    assert(strcmp(audio_latency_file_value(AudioLatency::High),   "high")   == 0);

    // Round-trip.
    Settings als; als.audio_latency = AudioLatency::High;
    char albuf[512];
    serialize_settings(als, albuf, sizeof albuf);
    assert(parse_settings(albuf).audio_latency == AudioLatency::High);
```

- [ ] **Step 2: Run the test to verify it fails (does not compile)**

Run: `cd test && make test_settings`
Expected: FAIL — compile error, `'AudioLatency' was not declared` / `audio_latency_frames was not declared`.

- [ ] **Step 3: Add the enum, field, and ctor init in `settings.h`**

After the line `enum class AudioOutput { HDMI, Analog };` add:

```cpp
enum class AudioLatency { Low, Medium, High };
```

In `struct Settings`, after the `bool debug_overlay;` line, add:

```cpp
    AudioLatency audio_latency;   // audio buffering depth: low | medium | high
```

In the constructor initializer list, change the tail `debug_overlay(false)` to:

```cpp
        debug_overlay(false), audio_latency(AudioLatency::Medium)
```

- [ ] **Step 4: Declare the two helpers in `settings.h`**

After the `audio_output_file_value` declaration (near line 119), add:

```cpp
// AudioLatency as written to the settings file ("low" | "medium" | "high").
const char *audio_latency_file_value(AudioLatency l);

// Target buffered-audio depth as a video-frame multiplier (Low=1, Medium=2,
// High=3); the kernel computes target = audio_latency_frames(l) * framesPerVideo.
unsigned audio_latency_frames(AudioLatency l);
```

- [ ] **Step 5: Implement parse + serialize + helpers in `settings.cpp`**

In `parse_settings`, after the `audio_output` branch (the block ending `: AudioOutput::HDMI;`), add:

```cpp
        else if (ieq(key, "audio_latency"))
        {
            if      (ieq(val, "low"))  s.audio_latency = AudioLatency::Low;
            else if (ieq(val, "high")) s.audio_latency = AudioLatency::High;
            else                       s.audio_latency = AudioLatency::Medium;
        }
```

After the `audio_output_file_value` function definition (near line 317), add:

```cpp
const char *audio_latency_file_value(AudioLatency l)
{
    switch (l)
    {
    case AudioLatency::Low:  return "low";
    case AudioLatency::High: return "high";
    default:                 return "medium";
    }
}

unsigned audio_latency_frames(AudioLatency l)
{
    switch (l)
    {
    case AudioLatency::Low:  return 1;
    case AudioLatency::High: return 3;
    default:                 return 2;   // Medium == today's behavior
    }
}
```

In `serialize_settings`, after the `audio_output=` block (the two `appendz` lines ending with `audio_output_file_value(s.audio_output)`), add:

```cpp
    appendz(out, out_size, "\naudio_latency=");
    appendz(out, out_size, audio_latency_file_value(s.audio_latency));
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: PASS — `All settings tests passed`.

- [ ] **Step 7: Run the full host test suite (no regressions)**

Run: `cd test && make && ./test_settings && ./test_joypad && ./test_pad_reconcile`
Expected: each prints its pass line (`All settings tests passed`, `All joypad tests passed`, `test_pad_reconcile: OK`).

- [ ] **Step 8: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add audio_latency field + parse/serialize + frame helper

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Kernel live pacing target

**Files:**
- Modify: `src/kernel.cpp` (remove the per-ROM-load `target` at line ~248; add a live `target` inside the play loop, before the HUD/gate use it — around line ~370, before the `if (m_Overlay.Enabled ())` block)

**Interfaces:**
- Consumes: `audio_latency_frames(AudioLatency)` and `Settings::audio_latency` (Task 1); existing `framesPerVideo`, `m_Settings`.
- Produces: a per-frame `unsigned target` that scales with `m_Settings.audio_latency`; the watermark expression `target + framesPerVideo` is unchanged.

- [ ] **Step 1: Remove the static per-ROM-load target**

In `src/kernel.cpp`, delete this line (near line 248, in the "Pacing parameters" block):

```cpp
		unsigned target         = framesPerVideo * 2;
```

(Leave `framesPerVideo` and `period_us` as they are.)

- [ ] **Step 2: Add the live target inside the play loop**

Inside the `for (;;)` play loop, immediately after `m_Sram.Tick ();` and before the FPS-measurement block (i.e. right after line ~358 `m_Sram.Tick ();`), add:

```cpp
			// Audio buffering depth from the live latency setting (Medium == the
			// historical framesPerVideo*2). The gate stays one frame above target.
			unsigned target = audio_latency_frames (m_Settings.audio_latency)
			                  * framesPerVideo;
```

This is in scope for both the HUD use (`st.target = target;`) and the gate (`classify_queue (q, 0, target + framesPerVideo)` and `while (... > target + framesPerVideo)`), which already read `target` and are unchanged.

- [ ] **Step 3: Verify the helper is visible**

Run: `grep -n "settings/settings.h\|audio_latency_frames" src/kernel.cpp src/kernel.h`
Expected: `settings.h` is already included transitively (the kernel uses `Settings`, `video_mode_file_value`, etc.). If the build in Step 4 reports `audio_latency_frames was not declared`, add `#include "settings/settings.h"` near the other includes at the top of `src/kernel.cpp` and rebuild.

- [ ] **Step 4: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion, produces `kernel7.img`, no errors. (A `-Wreorder` or `-Werror` failure here means the Task 1 ctor init order is wrong — fix init-list order to match declaration order.)

- [ ] **Step 5: Commit**

```bash
git add src/kernel.cpp
git commit -m "Kernel: scale audio pacing target by the live audio_latency setting

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Settings screen row + hardware-verify checklist

**Files:**
- Modify: `src/menu/settings_screen.cpp` (NUM_ROWS at line 16; `latVal` + labels/values arrays at lines ~91-104; new `case 10` in the dir switch ~line 220; renumber the GP_START action indices at lines 229/235/241)
- Modify: `docs/hardware-verification-checklist-2026-06-20.md` (append a section)

**Interfaces:**
- Consumes: `Settings::audio_latency`, `AudioLatency` (Task 1); existing `m_pSettings`, `m_pStore`, `Apply()`.
- Produces: an `Audio Latency: < Low | Medium | High >` cycle row at index 10; action rows shift to Controls=11, Video Mode=12, Hotkeys=13.

- [ ] **Step 1: Bump the row count**

In `src/menu/settings_screen.cpp`, change:

```cpp
#define NUM_ROWS 13
```

to:

```cpp
#define NUM_ROWS 14
```

- [ ] **Step 2: Add the value string**

In `Render`, after the `dbgVal` line (near line 94), add:

```cpp
    const char *latVal =
        m_pSettings->audio_latency == AudioLatency::Low  ? "< Low >"  :
        m_pSettings->audio_latency == AudioLatency::High ? "< High >" :
                                                           "< Medium >";
```

- [ ] **Step 3: Insert the label and value into the arrays**

Change the `labels[NUM_ROWS]` initializer so `"Audio Latency:"` sits between `"Debug Overlay:"` and `"Controls..."`:

```cpp
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Audio out:",
                                     "Vsync:", "Debug Overlay:",
                                     "Audio Latency:",
                                     "Controls...", "Video Mode...",
                                     "Hotkeys..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, audioVal,
                                     vsyncVal, dbgVal, latVal, "", "", "" };
```

- [ ] **Step 4: Add the cycle handler**

In the `dir != 0` switch in `Run`, after `case 9:` (Debug Overlay, which ends with `m_pSettings->debug_overlay = !m_pSettings->debug_overlay;` then `break;`), add:

```cpp
            case 10:  // Audio Latency (cycle Low -> Medium -> High; live)
            {
                int l = (int) m_pSettings->audio_latency + dir;
                if (l < (int) AudioLatency::Low)  l = (int) AudioLatency::Low;
                if (l > (int) AudioLatency::High) l = (int) AudioLatency::High;
                m_pSettings->audio_latency = (AudioLatency) l;
                break;
            }
```

(`Apply()` + `m_pStore->Save(*m_pSettings)` already run after the switch; the kernel reads `audio_latency` live, so no change to `Apply()` is needed.)

- [ ] **Step 5: Renumber the action rows**

In the `pressed & GP_START` block, update the three indices (each shifted by +1):

```cpp
            if (selected == 11)                       // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 12)                  // Video Mode...
            {
                m_pVideoMode->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 13)                  // Hotkeys...
            {
                m_pHotkey->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
```

- [ ] **Step 6: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion, `kernel7.img` produced, no errors.

- [ ] **Step 7: Append the hardware-verify checklist section**

In `docs/hardware-verification-checklist-2026-06-20.md`, before the `## Results summary` section, add:

```markdown
## P. Audio latency presets

- [ ] **P1 — Live change.** Pause → Settings → **Audio Latency**. Left/Right cycles
  Low / Medium / High; selecting applies on resume without reboot.
- [ ] **P2 — Low is clean.** Set Low on a demanding game. **Expect:** audio stays
  clean — the underrun (U) counter in the HUD / ~5 s log does not climb steadily.
- [ ] **P3 — High reflects in HUD.** Set High. **Expect:** the HUD `target` value
  rises (more frames buffered); audio still clean.
- [ ] **P4 — Medium == before.** Set Medium. **Expect:** behaves exactly as prior
  to this feature.
- [ ] **P5 — Persisted.** Confirm `audio_latency` in `SD:/settings.txt`; survives reboot.
```

Also add a row to the results-summary table (after `| O. Hotkey remapping | | |`):

```markdown
| P. Audio latency | | |
```

- [ ] **Step 8: Commit**

```bash
git add src/menu/settings_screen.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Settings UI: Audio Latency row (low/medium/high) + hardware checklist

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes for the implementer

- The host test suite lives in `test/` and builds natively (no cross toolchain): `cd test && make`.
- The kernel build (`make` at repo root) is the only verification for `kernel.cpp` and `settings_screen.cpp` — there are no host tests for Circle-dependent code, consistent with the rest of the project.
- Do not touch `src/audio/*` or `AudioDriver::QUEUE_MS`; this feature is purely the kernel-side pacing target plus settings plumbing.
</content>
