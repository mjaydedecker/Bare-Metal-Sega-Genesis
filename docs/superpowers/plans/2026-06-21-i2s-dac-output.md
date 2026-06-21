# I2S DAC (PCM5102) Audio Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an `i2s` value to the `audio_output` setting that drives a PCM5102-class I2S DAC HAT as a third audio back end alongside HDMI and PWM/analog.

**Architecture:** Extend the `AudioOutput` enum to three values; add a third branch constructing `CI2SSoundBaseDevice` in `AudioDriver::Initialize()` (no I2C, default args — PCM5102 needs no codec init); make the Settings "Audio out" row a 3-way cycle. No change to the pacing loop, the core, or the volume/mute gain path.

**Tech Stack:** C++ (bare-metal, Circle), GNU make. Host unit tests compile/run natively in `test/`. Kernel cross-compiles to `kernel7.img` via repo-root `make`. The Circle I2S device object is already in the linked sound lib (`libs/circle/lib/sound/i2ssoundbasedevice.o`) — no Circle rebuild.

## Global Constraints

- Pure settings code (`src/settings/*`) has **no Circle dependencies** — stays host-compilable (included by `test/test_settings.cpp`).
- Default `audio_output` stays `HDMI`; unknown/missing file value → `HDMI`.
- File keyword/label: value `i2s`, on-screen label `< I2S DAC >`.
- Scope is the **no-I2C DAC** case only: construct `CI2SSoundBaseDevice(m_pInterrupt, nSampleRate)` — all I2S-specific args default (`nChunkSize=8192`, `bSlave=FALSE`, `pI2CMaster=0`). Do **not** add a `CI2CMaster` or resampling.
- Applies on **reboot** (device created once at boot), like the existing analog/HDMI selection. No runtime switching.
- Do **not** pre-tune `nChunkSize`; leave the default and flag choppiness as a hardware-verify check.
- Enum order is `{ HDMI, Analog, I2S }` (HDMI=0, Analog=1, I2S=2) — the Settings cycle and clamps depend on this.

---

### Task 1: Settings model — `AudioOutput::I2S` (host-tested)

**Files:**
- Modify: `src/settings/settings.h` (enum near line 19)
- Modify: `src/settings/settings.cpp` (parse `audio_output` branch near line 127; `audio_output_file_value` near line 314)
- Test: `test/test_settings.cpp` (extend the audio-output block near line 203)

**Interfaces:**
- Produces:
  - `enum class AudioOutput { HDMI, Analog, I2S };` (extends the existing two-value enum)
  - file value `"i2s"` ↔ `AudioOutput::I2S`
  - `audio_output_file_value(AudioOutput::I2S) == "i2s"`

- [ ] **Step 1: Write the failing tests**

In `test/test_settings.cpp`, immediately after the line
`assert(parse_settings(aobuf).audio_output == AudioOutput::Analog);`
(the end of the existing audio-output block), insert:

```cpp
    // I2S output parses, file value, round-trips.
    assert(parse_settings("audio_output=i2s\n").audio_output == AudioOutput::I2S);
    assert(strcmp(audio_output_file_value(AudioOutput::I2S), "i2s") == 0);
    Settings i2ss; i2ss.audio_output = AudioOutput::I2S;
    char i2sbuf[512];
    serialize_settings(i2ss, i2sbuf, sizeof i2sbuf);
    assert(parse_settings(i2sbuf).audio_output == AudioOutput::I2S);
```

- [ ] **Step 2: Run the test to verify it fails (does not compile)**

Run: `cd test && make test_settings`
Expected: FAIL — `'I2S' is not a member of 'AudioOutput'`.

- [ ] **Step 3: Extend the enum in `settings.h`**

Change:

```cpp
enum class AudioOutput { HDMI, Analog };
```

to:

```cpp
enum class AudioOutput { HDMI, Analog, I2S };
```

- [ ] **Step 4: Update parse + serialize in `settings.cpp`**

Replace the `audio_output` parse branch:

```cpp
        else if (ieq(key, "audio_output"))
            s.audio_output = ieq(val, "analog") ? AudioOutput::Analog
                                                : AudioOutput::HDMI;
```

with:

```cpp
        else if (ieq(key, "audio_output"))
        {
            if      (ieq(val, "analog")) s.audio_output = AudioOutput::Analog;
            else if (ieq(val, "i2s"))    s.audio_output = AudioOutput::I2S;
            else                         s.audio_output = AudioOutput::HDMI;
        }
```

Replace the helper:

```cpp
const char *audio_output_file_value(AudioOutput o)
{
    return o == AudioOutput::Analog ? "analog" : "hdmi";
}
```

with:

```cpp
const char *audio_output_file_value(AudioOutput o)
{
    switch (o)
    {
    case AudioOutput::Analog: return "analog";
    case AudioOutput::I2S:    return "i2s";
    default:                  return "hdmi";
    }
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: PASS — `All settings tests passed`.

- [ ] **Step 6: Run the full host suite (no regressions)**

Run: `cd test && make && ./test_settings && ./test_joypad && ./test_pad_reconcile`
Expected: `All settings tests passed`, `All joypad tests passed`, `test_pad_reconcile: OK`.

- [ ] **Step 7: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add AudioOutput::I2S value + parse/serialize

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: AudioDriver I2S device branch

**Files:**
- Modify: `src/audio/audio_driver.h` (sound-device includes near lines 12-16)
- Modify: `src/audio/audio_driver.cpp` (`Initialize()` device selection near lines 41-44)

**Interfaces:**
- Consumes: `AudioOutput::I2S` (Task 1).
- Produces: an `AudioDriver` that constructs `CI2SSoundBaseDevice` when `out == AudioOutput::I2S`; all downstream queue/format/start logic unchanged.

- [ ] **Step 1: Add the I2S device header include**

In `src/audio/audio_driver.h`, after the line
`#include <circle/sound/pwmsoundbasedevice.h>`, add:

```cpp
#include <circle/sound/i2ssoundbasedevice.h>
```

- [ ] **Step 2: Add the I2S branch in `Initialize()`**

In `src/audio/audio_driver.cpp`, replace:

```cpp
    if (out == AudioOutput::Analog)
        m_pDevice = new CPWMSoundBaseDevice(m_pInterrupt, nSampleRate);
    else
        m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, nSampleRate);
```

with:

```cpp
    if (out == AudioOutput::Analog)
        m_pDevice = new CPWMSoundBaseDevice(m_pInterrupt, nSampleRate);
    else if (out == AudioOutput::I2S)
        // PCM5102-class DAC: Pi is I2S master, no codec I2C init needed, so all
        // I2S-specific ctor args (nChunkSize, bSlave, pI2CMaster) take defaults.
        m_pDevice = new CI2SSoundBaseDevice(m_pInterrupt, nSampleRate);
    else
        m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, nSampleRate);
```

- [ ] **Step 3: Build the kernel**

Run: `make` (from the repo root)
Expected: compiles and links to completion; produces `kernel7.img`; no
`undefined reference to CI2SSoundBaseDevice` (the object is already in the linked
sound lib). If the link fails on the I2S symbol, confirm
`libs/circle/lib/sound/i2ssoundbasedevice.o` is part of the sound library archive.

- [ ] **Step 4: Commit**

```bash
git add src/audio/audio_driver.h src/audio/audio_driver.cpp
git commit -m "Audio: construct CI2SSoundBaseDevice for AudioOutput::I2S

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Settings screen 3-way "Audio out" row + hardware checklist

**Files:**
- Modify: `src/menu/settings_screen.cpp` (`audioVal` near lines 91-92; case 7 in the dir switch near lines 210-214)
- Modify: `docs/hardware-verification-checklist-2026-06-20.md` (extend section J)

**Interfaces:**
- Consumes: `AudioOutput::I2S`, `audio_output` (Task 1).
- Produces: an "Audio out" row that cycles HDMI → Analog → I2S DAC and displays `< I2S DAC >`.

- [ ] **Step 1: Make the display value three-way**

In `src/menu/settings_screen.cpp`, replace:

```cpp
    const char *audioVal = m_pSettings->audio_output == AudioOutput::Analog
                               ? "< Analog >" : "< HDMI >";
```

with:

```cpp
    const char *audioVal =
        m_pSettings->audio_output == AudioOutput::Analog ? "< Analog >"  :
        m_pSettings->audio_output == AudioOutput::I2S    ? "< I2S DAC >" :
                                                           "< HDMI >";
```

- [ ] **Step 2: Make the edit handler cycle three values**

Replace the case 7 body:

```cpp
            case 7:   // Audio out (toggle HDMI <-> Analog; applies on reboot)
                m_pSettings->audio_output =
                    m_pSettings->audio_output == AudioOutput::HDMI
                        ? AudioOutput::Analog : AudioOutput::HDMI;
                break;
```

with:

```cpp
            case 7:   // Audio out (cycle HDMI -> Analog -> I2S; applies on reboot)
            {
                int a = (int) m_pSettings->audio_output + dir;
                if (a < 0) a = 2;
                if (a > 2) a = 0;
                m_pSettings->audio_output = (AudioOutput) a;
                break;
            }
```

- [ ] **Step 3: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion; `kernel7.img` produced; no errors.

- [ ] **Step 4: Extend the hardware-verify checklist (section J)**

In `docs/hardware-verification-checklist-2026-06-20.md`, find the
`## J. Analog (3.5mm) audio output` section and insert these items immediately
before that section's `---` divider (after the existing `J4` line):

```markdown
- [ ] **J5 — I2S DAC output.** Connect a PCM5102 I2S DAC HAT. Settings → **Audio
  out** = I2S DAC → reboot. **Expect:** game audio comes from the DAC; HDMI and
  the 3.5 mm jack are silent. Confirm `audio_output=i2s` in `SD:/settings.txt`.
- [ ] **J6 — I2S volume/mute + clean.** With I2S selected, change volume and
  toggle mute (they should scale/silence the DAC like the other outputs).
  **Expect:** audio is clean — no choppiness/dropouts. If choppy, note it: the
  fix is a smaller explicit `nChunkSize` in the `CI2SSoundBaseDevice` ctor.
- [ ] **J7 — Back to HDMI.** Set Audio out = HDMI, reboot. **Expect:** audio
  returns to HDMI; `audio_output=hdmi` in the file.
```

- [ ] **Step 5: Commit**

```bash
git add src/menu/settings_screen.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Settings UI: 3-way Audio out (HDMI/Analog/I2S DAC) + hardware checks

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes for the implementer

- Host tests live in `test/` and build natively: `cd test && make`.
- The kernel build (`make` at repo root) is the only verification for the
  Circle-dependent changes (`audio_driver.cpp`, `settings_screen.cpp`), consistent
  with the rest of the project.
- Do not add a `CI2CMaster`, resampling, or runtime output switching — all out of
  scope (I2C-configured DACs are a deferred follow-on).
</content>
