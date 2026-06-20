# Audio Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add user-facing master volume + mute and internal underrun/overrun audio-queue metrics, reusing the existing settings subsystem.

**Architecture:** Volume/mute are applied at the single `AudioDriver::Write()` choke point (mute writes silence, never skips, so queue pacing stays intact). Pure host-tested helpers (`scale_sample`, `classify_queue`) hold the logic; `AudioDriver` holds volume/mute state + metric counters; the kernel classifies the queue each frame and logs counts periodically. Two rows are added to the existing Settings screen.

**Tech Stack:** C++ (bare-metal, Circle framework), Genesis-Plus-GX-Wide libretro core, host-side g++ unit tests.

## Global Constraints

- Pure logic files (`src/audio/audio_util.{h,cpp}`, `src/settings/settings.{h,cpp}`) MUST have **no Circle dependencies** (only `<stdint.h>`/`<stddef.h>`/`<string.h>`) so they compile with the host test toolchain.
- Volume is an integer **0–100**; values are clamped to that range on parse and on adjust.
- **Mute and volume==0 write silence (zeroed samples) of the same frame count — never skip the write.** Skipping starves the audio queue and breaks frame pacing.
- Build with `-Wall -Wextra`: constructor member-init order MUST match declaration order (`-Wreorder`).
- Every new `.cpp` under `src/` MUST be added to `OBJS` in the top-level `Makefile` (the `src/audio/` directory is already in `EXTRACLEAN`).
- New host test sources go in `test/`, are added to `test/Makefile`'s `run`/`clean` targets, and their built binaries added to `.gitignore`.
- Follow existing code style: 4-space indent, `m_`-prefixed members, header guards `_dir_name_h`, RGB565 colour constants in UI.

---

## File Structure

**New files:**
- `src/audio/audio_util.h` / `.cpp` — pure `scale_sample` + `classify_queue` + `AudioQueueEvent`.
- `test/test_audio_util.cpp` — host unit tests.

**Modified files:**
- `src/settings/settings.h` / `.cpp` — add `volume` / `mute` fields + parse/serialize.
- `test/test_settings.cpp` — cover `volume` / `mute`.
- `src/audio/audio_driver.h` / `.cpp` — volume/mute state + setters, gain in `Write`, metric counters.
- `src/menu/settings_screen.h` / `.cpp` — `AudioDriver*`, Volume + Mute rows.
- `src/kernel.h` / `.cpp` — pass `&m_Audio` to the Settings screen, boot apply, per-frame metrics + periodic log.
- `Makefile`, `test/Makefile`, `.gitignore`.

---

## Task 1: Pure audio helpers (scale_sample + classify_queue)

**Files:**
- Create: `src/audio/audio_util.h`
- Create: `src/audio/audio_util.cpp`
- Create: `test/test_audio_util.cpp`
- Modify: `test/Makefile`, `.gitignore`, `Makefile`

**Interfaces:**
- Produces:
  - `int16_t scale_sample(int16_t sample, unsigned volume, bool mute);`
  - `enum AudioQueueEvent { AQ_None, AQ_Underrun, AQ_Overrun };`
  - `AudioQueueEvent classify_queue(unsigned frames, unsigned low, unsigned high);`

- [ ] **Step 1: Write the header**

Create `src/audio/audio_util.h`:

```cpp
//
// src/audio/audio_util.h
//
// Bare Metal Sega Genesis
// Pure audio helpers: per-sample volume/mute gain and audio-queue-depth
// classification. No Circle dependencies — host-testable.
//

#ifndef _audio_audio_util_h
#define _audio_audio_util_h

#include <stdint.h>

// Scale one signed-16 audio sample by a 0-100 volume percentage. mute or
// volume==0 yields 0; volume>=100 returns the sample unchanged. Scaling down
// never overflows int16.
int16_t scale_sample(int16_t sample, unsigned volume, bool mute);

enum AudioQueueEvent { AQ_None, AQ_Underrun, AQ_Overrun };

// Classify an audio-queue depth: frames <= low => AQ_Underrun (starved),
// frames > high => AQ_Overrun (too full), otherwise AQ_None.
AudioQueueEvent classify_queue(unsigned frames, unsigned low, unsigned high);

#endif
```

- [ ] **Step 2: Write the failing test**

Create `test/test_audio_util.cpp`:

```cpp
#include "../src/audio/audio_util.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // scale_sample
    assert(scale_sample(1000, 100, false) == 1000);   // identity
    assert(scale_sample(1000, 0,   false) == 0);       // zero volume
    assert(scale_sample(1000, 100, true)  == 0);       // mute overrides
    assert(scale_sample(1000, 50,  false) == 500);     // half
    assert(scale_sample(-2000, 50, false) == -1000);   // negative
    assert(scale_sample(32767, 100, false) == 32767);  // max identity
    assert(scale_sample(-32768, 100, false) == -32768);
    assert(scale_sample(-32768, 50, false) == -16384);
    assert(scale_sample(1234, 150, false) == 1234);    // >100 clamps to identity

    // classify_queue (low=0, high=100)
    assert(classify_queue(0,   0, 100) == AQ_Underrun);
    assert(classify_queue(50,  0, 100) == AQ_None);
    assert(classify_queue(100, 0, 100) == AQ_None);    // == high is fine
    assert(classify_queue(101, 0, 100) == AQ_Overrun);
    assert(classify_queue(5,  10, 100) == AQ_Underrun);// low threshold > 0

    printf("All audio_util tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Wire the test into `test/Makefile`**

Add `test_audio_util` to the `run` prerequisites and commands:

```makefile
run: test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings test_audio_util
	./test_blit
	./test_joypad
	./test_rom_filter
	./test_menu_state
	./test_menu_path
	./test_save_path
	./test_settings
	./test_audio_util
```

Add the build rule after the `test_settings` rule:

```makefile
test_audio_util: test_audio_util.cpp ../src/audio/audio_util.cpp ../src/audio/audio_util.h
	$(CXX) $(CXXFLAGS) -o $@ test_audio_util.cpp ../src/audio/audio_util.cpp
```

Update `clean`:

```makefile
clean:
	rm -f test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings test_audio_util
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `cd test && make test_audio_util`
Expected: FAIL — `No rule to make target '../src/audio/audio_util.cpp'` (file not created yet).

- [ ] **Step 5: Write the implementation**

Create `src/audio/audio_util.cpp`:

```cpp
//
// src/audio/audio_util.cpp
//
// Bare Metal Sega Genesis
// See audio_util.h.
//

#include "audio_util.h"

int16_t scale_sample(int16_t sample, unsigned volume, bool mute)
{
    if (mute || volume == 0) return 0;
    if (volume >= 100)       return sample;
    return (int16_t) (((int32_t) sample * (int32_t) volume) / 100);
}

AudioQueueEvent classify_queue(unsigned frames, unsigned low, unsigned high)
{
    if (frames <= low) return AQ_Underrun;
    if (frames > high) return AQ_Overrun;
    return AQ_None;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd test && make test_audio_util && ./test_audio_util`
Expected: `All audio_util tests passed`

- [ ] **Step 7: Add the object to the top-level `Makefile`**

In `Makefile`, add to `OBJS` after `src/audio/audio_driver.o`:

```makefile
       src/audio/audio_util.o \
```

- [ ] **Step 8: Add the test binary to `.gitignore`**

In `.gitignore`, under the host test binaries list, add:

```
/test/test_audio_util
```

- [ ] **Step 9: Build the kernel to verify it links**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 10: Commit**

```bash
git add src/audio/audio_util.h src/audio/audio_util.cpp test/test_audio_util.cpp test/Makefile .gitignore Makefile
git commit -m "Audio: pure scale_sample + classify_queue helpers"
```

---

## Task 2: Settings model — volume + mute

**Files:**
- Modify: `src/settings/settings.h`
- Modify: `src/settings/settings.cpp`
- Modify: `test/test_settings.cpp`

**Interfaces:**
- Consumes: existing `Settings`, `parse_settings`, `serialize_settings`.
- Produces: `Settings` gains `unsigned volume;` (default 100) and `bool mute;` (default false); file keys `volume` / `mute`.

- [ ] **Step 1: Write the failing test additions**

In `test/test_settings.cpp`, add these assertions immediately before the
`printf("All settings tests passed\n");` line:

```cpp
    // Defaults for the new audio fields.
    assert(d.volume == 100);
    assert(d.mute == false);

    // Parse + clamp volume; mute truthy.
    Settings v = parse_settings("volume=70\nmute=on\n");
    assert(v.volume == 70);
    assert(v.mute == true);
    Settings vc = parse_settings("volume=250\n");      // clamps to 100
    assert(vc.volume == 100);
    Settings vb = parse_settings("volume=bogus\n");    // non-numeric -> default
    assert(vb.volume == 100);

    // Round-trip includes volume + mute.
    Settings as; as.volume = 30; as.mute = true;
    char abuf[256];
    serialize_settings(as, abuf, sizeof abuf);
    Settings art = parse_settings(abuf);
    assert(art.volume == 30);
    assert(art.mute == true);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_settings`
Expected: FAIL — compile error: `'struct Settings' has no member named 'volume'`.

- [ ] **Step 3: Add the fields to `settings.h`**

In `src/settings/settings.h`, replace the `Settings` struct with:

```cpp
struct Settings
{
    ScaleMode scale_mode;   // video_scale: integer | stretch
    bool      widescreen;   // widescreen:  on | off
    unsigned  volume;       // 0-100 master volume
    bool      mute;         // audio mute

    Settings(void)
    :   scale_mode(ScaleMode::Integer), widescreen(false),
        volume(100), mute(false) {}
};
```

- [ ] **Step 4: Parse the new keys in `settings.cpp`**

In `src/settings/settings.cpp`, in `parse_settings`, replace the
`else if (ieq(key, "widescreen"))` clause with:

```cpp
        else if (ieq(key, "widescreen"))
            s.widescreen = truthy(val);
        else if (ieq(key, "volume"))
        {
            unsigned v = 0;
            bool any = false;
            for (const char *q = val; *q >= '0' && *q <= '9'; q++)
            {
                v = v * 10 + (unsigned) (*q - '0');
                any = true;
                if (v > 100000) break;        // guard against silly input
            }
            if (any)
            {
                if (v > 100) v = 100;
                s.volume = v;
            }
            // non-numeric: keep default
        }
        else if (ieq(key, "mute"))
            s.mute = truthy(val);
```

- [ ] **Step 5: Serialize the new keys in `settings.cpp`**

In `src/settings/settings.cpp`, add this unsigned-to-text helper just above
`void serialize_settings(...)`:

```cpp
// Append an unsigned integer as decimal text.
static void append_uint(char *out, size_t out_size, unsigned v)
{
    char rev[12];
    int  n = 0;
    if (v == 0) rev[n++] = '0';
    else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
    char fwd[12];
    int  m = 0;
    while (n) fwd[m++] = rev[--n];
    fwd[m] = '\0';
    appendz(out, out_size, fwd);
}
```

Then in `serialize_settings`, replace the widescreen tail:

```cpp
    appendz(out, out_size, "\nwidescreen=");
    appendz(out, out_size, s.widescreen ? "on" : "off");
    appendz(out, out_size, "\n");
```

with:

```cpp
    appendz(out, out_size, "\nwidescreen=");
    appendz(out, out_size, s.widescreen ? "on" : "off");
    appendz(out, out_size, "\nvolume=");
    append_uint(out, out_size, s.volume);
    appendz(out, out_size, "\nmute=");
    appendz(out, out_size, s.mute ? "on" : "off");
    appendz(out, out_size, "\n");
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `cd test && make test_settings && ./test_settings`
Expected: `All settings tests passed`

- [ ] **Step 7: Build the kernel to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 8: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add volume + mute fields"
```

---

## Task 3: AudioDriver — volume/mute gain + metric counters

**Files:**
- Modify: `src/audio/audio_driver.h`
- Modify: `src/audio/audio_driver.cpp`

**Interfaces:**
- Consumes: `scale_sample` (Task 1).
- Produces (on `AudioDriver`):
  - `void SetVolume(unsigned volume);` (clamps to 0–100)
  - `void SetMute(bool mute);`
  - `void RecordUnderrun();` / `void RecordOverrun();`
  - `unsigned Underruns() const;` / `unsigned Overruns() const;`

- [ ] **Step 1: Declare state, setters, and counters in the header**

In `src/audio/audio_driver.h`, in the `public:` section after
`boolean IsReady(void) const;`, add:

```cpp
    // Master volume 0-100 and mute. Safe to call before Initialize() — the
    // values are applied when Write() runs.
    void SetVolume(unsigned volume);   // clamped to 0-100
    void SetMute(bool mute);

    // Underrun/overrun metrics (bumped by the kernel from the pacing loop).
    void     RecordUnderrun(void) { m_Underruns++; }
    void     RecordOverrun (void) { m_Overruns++;  }
    unsigned Underruns(void) const { return m_Underruns; }
    unsigned Overruns (void) const { return m_Overruns;  }
```

In the `private:` section, replace the member list:

```cpp
private:
    CInterruptSystem     *m_pInterrupt;
    CHDMISoundBaseDevice *m_pDevice;
```

with:

```cpp
private:
    static const unsigned STAGE_FRAMES = 1024;   // gain staging chunk size

    CInterruptSystem     *m_pInterrupt;
    CHDMISoundBaseDevice *m_pDevice;
    unsigned              m_Volume;     // 0-100
    bool                  m_Mute;
    unsigned              m_Underruns;
    unsigned              m_Overruns;
```

- [ ] **Step 2: Initialize the new members and add setters**

In `src/audio/audio_driver.cpp`, add the include after the existing includes:

```cpp
#include "audio_util.h"
```

Replace the constructor:

```cpp
AudioDriver::AudioDriver(CInterruptSystem *pInterrupt)
:   m_pInterrupt(pInterrupt), m_pDevice(0)
{
}
```

with:

```cpp
AudioDriver::AudioDriver(CInterruptSystem *pInterrupt)
:   m_pInterrupt(pInterrupt), m_pDevice(0),
    m_Volume(100), m_Mute(false), m_Underruns(0), m_Overruns(0)
{
}

void AudioDriver::SetVolume(unsigned volume)
{
    m_Volume = volume > 100 ? 100 : volume;
}

void AudioDriver::SetMute(bool mute)
{
    m_Mute = mute;
}
```

- [ ] **Step 3: Apply gain in `Write`**

In `src/audio/audio_driver.cpp`, replace the whole `Write` method:

```cpp
void AudioDriver::Write(const s16 *pSamples, unsigned nFrames)
{
    if (m_pDevice != 0 && nFrames > 0)
    {
        // 4 bytes per signed-16 stereo frame; Write() takes a byte count.
        m_pDevice->Write(pSamples, (size_t) nFrames * 4);
    }
}
```

with:

```cpp
void AudioDriver::Write(const s16 *pSamples, unsigned nFrames)
{
    if (m_pDevice == 0 || nFrames == 0)
    {
        return;
    }

    // Fast path: full volume, not muted -> write the core's buffer directly.
    if (!m_Mute && m_Volume >= 100)
    {
        m_pDevice->Write(pSamples, (size_t) nFrames * 4);
        return;
    }

    // Scaled / muted path: stage gained samples in chunks (2 s16 per frame).
    // Always writes nFrames (silence when muted) so queue pacing is unaffected.
    static s16 staging[STAGE_FRAMES * 2];
    unsigned done = 0;
    while (done < nFrames)
    {
        unsigned chunk = nFrames - done;
        if (chunk > STAGE_FRAMES) chunk = STAGE_FRAMES;

        const s16 *in = pSamples + (size_t) done * 2;
        for (unsigned i = 0; i < chunk * 2; i++)
        {
            staging[i] = scale_sample(in[i], m_Volume, m_Mute);
        }

        m_pDevice->Write(staging, (size_t) chunk * 4);
        done += chunk;
    }
}
```

- [ ] **Step 4: Build to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors or `-Wreorder` warnings.

- [ ] **Step 5: Commit**

```bash
git add src/audio/audio_driver.h src/audio/audio_driver.cpp
git commit -m "Audio: volume/mute gain in Write + metric counters"
```

---

## Task 4: Settings screen — Volume + Mute rows

**Files:**
- Modify: `src/menu/settings_screen.h`
- Modify: `src/menu/settings_screen.cpp`

**Interfaces:**
- Consumes: `AudioDriver::SetVolume/SetMute` (Task 3); `Settings.volume/mute` (Task 2).
- Produces: `SettingsScreen` constructor gains an `AudioDriver *pAudio` parameter (appended last).

- [ ] **Step 1: Add AudioDriver to the header**

In `src/menu/settings_screen.h`, add the include after
`#include "../video/display.h"`:

```cpp
#include "../audio/audio_driver.h"
```

Replace the constructor declaration:

```cpp
    SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay);
```

with:

```cpp
    SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay,
                   AudioDriver *pAudio);
```

In the `private:` member list, after `Display *m_pDisplay;`, add:

```cpp
    AudioDriver   *m_pAudio;
```

- [ ] **Step 2: Update the constructor and Apply()**

In `src/menu/settings_screen.cpp`, replace the constructor:

```cpp
SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay)
{
}
```

with:

```cpp
SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay,
                               AudioDriver *pAudio)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay),
    m_pAudio(pAudio)
{
}
```

Replace `Apply`:

```cpp
void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_variables_dirty = true;                            // ...on next poll/reset
}
```

with:

```cpp
void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_variables_dirty = true;                            // ...on next poll/reset
    m_pAudio->SetVolume(m_pSettings->volume);            // live
    m_pAudio->SetMute(m_pSettings->mute);                // live
}
```

- [ ] **Step 3: Add the volume formatter and grow Render to 4 rows**

In `src/menu/settings_screen.cpp`, change `#define NUM_ROWS 2` to:

```cpp
#define NUM_ROWS 4
```

Add this helper just above `void SettingsScreen::Render(int selected)`:

```cpp
// Format a 0-100 volume as "< NNN >" into out (>= 8 bytes).
static void fmt_volume(char *out, unsigned v)
{
    char rev[4];
    int  n = 0;
    if (v == 0) rev[n++] = '0';
    else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
    int i = 0;
    out[i++] = '<'; out[i++] = ' ';
    while (n) out[i++] = rev[--n];
    out[i++] = ' '; out[i++] = '>'; out[i] = '\0';
}
```

Replace the value/label setup in `Render` (the `scaleVal`/`wideVal`/`labels`/`values` block) with:

```cpp
    char volVal[8];
    fmt_volume(volVal, m_pSettings->volume);
    const char *scaleVal = m_pSettings->scale_mode == ScaleMode::Stretch
                               ? "< Stretch >" : "< Integer >";
    const char *wideVal  = m_pSettings->widescreen ? "< On >" : "< Off >";
    const char *muteVal  = m_pSettings->mute ? "< On >" : "< Off >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal };
```

- [ ] **Step 4: Handle direction-sensitive edits in Run()**

In `src/menu/settings_screen.cpp`, in `Run`, replace the whole
`if (pressed & (GP_LEFT | GP_RIGHT))` block:

```cpp
        if (pressed & (GP_LEFT | GP_RIGHT))
        {
            if (selected == 0)
                m_pSettings->scale_mode =
                    m_pSettings->scale_mode == ScaleMode::Integer
                        ? ScaleMode::Stretch : ScaleMode::Integer;
            else
                m_pSettings->widescreen = !m_pSettings->widescreen;

            Apply();
            m_pStore->Save(*m_pSettings);
            Render(selected);
        }
```

with:

```cpp
        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            switch (selected)
            {
            case 0:   // Video Scale (toggle, either direction)
                m_pSettings->scale_mode =
                    m_pSettings->scale_mode == ScaleMode::Integer
                        ? ScaleMode::Stretch : ScaleMode::Integer;
                break;
            case 1:   // Widescreen (toggle)
                m_pSettings->widescreen = !m_pSettings->widescreen;
                break;
            case 2:   // Volume (+/- 10, clamped 0-100)
            {
                int v = (int) m_pSettings->volume + dir * 10;
                if (v < 0)   v = 0;
                if (v > 100) v = 100;
                m_pSettings->volume = (unsigned) v;
                break;
            }
            case 3:   // Mute (toggle)
                m_pSettings->mute = !m_pSettings->mute;
                break;
            }

            Apply();
            m_pStore->Save(*m_pSettings);
            Render(selected);
        }
```

- [ ] **Step 5: Build to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`. (It will link even though the kernel still
calls the old 6-arg constructor only after Task 5; this task's file compiles on
its own — but the kernel call site is updated in Task 5, so a full `make` here
fails at the kernel. To verify just this file compiles, run the object build:)

Run: `make src/menu/settings_screen.o`
Expected: `CPP src/menu/settings_screen.o`, no errors.

- [ ] **Step 6: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp
git commit -m "Settings: Volume + Mute rows in the Settings screen"
```

---

## Task 5: Kernel wiring — boot apply + per-frame metrics + periodic log

**Files:**
- Modify: `src/kernel.h`
- Modify: `src/kernel.cpp`

**Interfaces:**
- Consumes: `SettingsScreen` 7-arg constructor (Task 4); `AudioDriver::SetVolume/SetMute/RecordUnderrun/RecordOverrun/Underruns/Overruns` (Task 3); `classify_queue` + `AQ_*` (Task 1).

- [ ] **Step 1: Include the audio helper in the kernel**

In `src/kernel.cpp`, add the include after `#include "input/joypad_map.h"`:

```cpp
#include "audio/audio_util.h"   // classify_queue, AQ_* for metrics
```

- [ ] **Step 2: Pass the audio driver to the Settings screen**

In `src/kernel.cpp`, in the constructor init list, replace:

```cpp
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display),
```

with:

```cpp
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display, &m_Audio),
```

- [ ] **Step 3: Apply volume/mute at boot**

In `src/kernel.cpp`, in `Run`, replace the settings-apply block:

```cpp
	// Load user settings and apply them before the core reads variables.
	m_SettingsStore.Load (&m_Settings);
	m_Display.SetScaleMode (m_Settings.scale_mode);
	g_widescreen = m_Settings.widescreen;
```

with:

```cpp
	// Load user settings and apply them before the core reads variables.
	m_SettingsStore.Load (&m_Settings);
	m_Display.SetScaleMode (m_Settings.scale_mode);
	g_widescreen = m_Settings.widescreen;
	m_Audio.SetVolume (m_Settings.volume);
	m_Audio.SetMute (m_Settings.mute);
```

- [ ] **Step 4: Add a metrics-log counter to the play loop locals**

In `src/kernel.cpp`, in the `// --- Play ---` locals, replace:

```cpp
		u64      next      = CTimer::GetClockTicks64 ();
		unsigned frame     = 0;
		boolean  ledOn     = FALSE;
		unsigned prevBtns  = 0;
		boolean  toBrowser = FALSE;
```

with:

```cpp
		u64      next      = CTimer::GetClockTicks64 ();
		unsigned frame     = 0;
		unsigned logCtr    = 0;
		boolean  ledOn     = FALSE;
		unsigned prevBtns  = 0;
		boolean  toBrowser = FALSE;
```

- [ ] **Step 5: Classify the queue each frame (replace the audio gate block)**

In `src/kernel.cpp`, replace the audio gate:

```cpp
			if (audioOK)
			{
				while (m_Audio.QueuedFrames () > target + framesPerVideo) { }
			}
```

with:

```cpp
			if (audioOK)
			{
				// Sample the queue depth before gating, classify for metrics,
				// then apply the high-watermark gate.
				unsigned q = m_Audio.QueuedFrames ();
				switch (classify_queue (q, 0, target + framesPerVideo))
				{
				case AQ_Underrun: m_Audio.RecordUnderrun (); break;
				case AQ_Overrun:  m_Audio.RecordOverrun ();  break;
				default: break;
				}
				while (m_Audio.QueuedFrames () > target + framesPerVideo) { }
			}
```

- [ ] **Step 6: Log the counters periodically (after the LED block)**

In `src/kernel.cpp`, replace the LED block:

```cpp
			if (++frame >= 30)
			{
				frame = 0;
				ledOn = !ledOn;
				if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
			}
```

with:

```cpp
			if (++frame >= 30)
			{
				frame = 0;
				ledOn = !ledOn;
				if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
			}

			if (++logCtr >= 300)   // ~5 s at 60 fps
			{
				logCtr = 0;
				if (audioOK)
				{
					m_Logger.Write (FromKernel, LogNotice,
						"audio underruns=%u overruns=%u",
						m_Audio.Underruns (), m_Audio.Overruns ());
				}
			}
```

- [ ] **Step 7: Build to verify everything compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors or `-Wreorder` warnings.

- [ ] **Step 8: Run the full host test suite**

Run: `cd test && make run`
Expected: all suites pass, including `All audio_util tests passed` and
`All settings tests passed`.

- [ ] **Step 9: Commit**

```bash
git add src/kernel.h src/kernel.cpp
git commit -m "Audio: wire volume/mute boot apply + queue metrics into the kernel"
```

---

## Hardware Verification (manual, after Task 5)

Not a code task — perform on the Pi per `docs/m6-hardware-setup.md`:

- [ ] In a game, open Settings (Start+Select → Settings). Adjust **Volume** down/up; confirm loudness changes audibly.
- [ ] Toggle **Mute** on; confirm silence **and** that playback stays smooth (no garble / no desync), then unmute and confirm sound returns.
- [ ] Boot with `logdev=ttyS1` on the kernel command line; over the serial console confirm `audio underruns=N overruns=M` logs every ~5 s with counts staying steady (not climbing) during normal play.
- [ ] Power-cycle; confirm `volume` / `mute` persisted in `SD:/settings.txt`.

---

## Self-Review Notes

- **Spec coverage:** choke-point gain + mute-writes-silence (Task 3); pure `scale_sample`/`classify_queue` (Task 1); `volume`/`mute` settings + clamp (Task 2); Settings-screen Volume/Mute rows + live Apply (Task 4); boot apply, per-frame classify, periodic serial log with the `logdev=ttyS1` caveat (Task 5); host tests (Tasks 1–2); hardware checklist incl. the smooth-mute and steady-counter checks. Output-device selection is out of scope per the spec — no task.
- **Type consistency:** `scale_sample`/`classify_queue`/`AudioQueueEvent` defined in Task 1 used unchanged in Tasks 3 & 5; `Settings.volume` (`unsigned`) / `mute` (`bool`) defined in Task 2 used in Tasks 4–5; `SetVolume`/`SetMute`/`RecordUnderrun`/`RecordOverrun`/`Underruns`/`Overruns` defined in Task 3 used in Tasks 4–5; `SettingsScreen` 7-arg constructor consistent between Task 4 and Task 5.
- **Note:** Task 4 Step 5 verifies via the single-object build (`make src/menu/settings_screen.o`) because the kernel call site is only updated in Task 5; a full `make` first succeeds at the end of Task 5.
