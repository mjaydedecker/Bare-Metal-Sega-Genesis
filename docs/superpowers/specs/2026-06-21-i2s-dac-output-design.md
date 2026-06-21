# I2S DAC (PCM5102) Audio Output — Design

**Date:** 2026-06-21
**Status:** approved design, pending implementation plan
**Related:** audio deferred-enhancements #1 (alternate/selectable output devices);
`docs/superpowers/specs/2026-06-16-audio-output-deferred-enhancements.md`;
[[project-analog-audio-output]] (the HDMI-vs-PWM selection this extends);
[[project-audio-settings]] (volume/mute gain step that applies to all back ends).

## Summary

Add a third audio output back end — an **I2S DAC HAT** (PCM5102-class, needing no
I2C codec configuration) — alongside the existing HDMI and PWM/analog outputs.
The output is chosen via the existing `audio_output` setting
(`SD:/settings.txt`), now a three-way choice `hdmi | analog | i2s`, selected on
the Settings screen and applied on reboot (the sound device is created once at
boot). This closes the remaining piece of deferred audio enhancement #1 (the
analog/PWM piece already shipped — [[project-analog-audio-output]]).

Scope is deliberately the **no-I2C DAC** case (PCM5102 / hardware-strapped
boards): the Pi is I2S master, the DAC needs no register init, and it accepts the
core's native 44.1 kHz, so no `CI2CMaster` plumbing and no resampling are
required.

## Background / Constraint

`AudioDriver::Initialize()` (`src/audio/audio_driver.cpp:34-68`) already hides the
physical device behind a small branch:

```cpp
if (out == AudioOutput::Analog)
    m_pDevice = new CPWMSoundBaseDevice(m_pInterrupt, nSampleRate);
else
    m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, nSampleRate);
```

All three Circle classes derive from `CSoundBaseDevice`, so the rest of
`AudioDriver` — `AllocateQueue(QUEUE_MS)`, `SetWriteFormat(SoundFormatSigned16,
2)`, `Write()`, `GetQueueFramesAvail()` — and the per-sample volume/mute gain step
in `Write()` work identically across back ends. Adding I2S is therefore a third
branch plus the settings plumbing; no change to the pacing loop, the core, or the
gain path.

The Circle I2S object is already compiled into the linked sound library
(`libs/circle/lib/sound/i2ssoundbasedevice.o`), so no Circle rebuild or new link
input is needed.

### The Circle I2S constructor

```cpp
CI2SSoundBaseDevice (CInterruptSystem *pInterrupt,
                     unsigned nSampleRate = 192000,
                     unsigned nChunkSize  = 8192,
                     bool     bSlave      = FALSE,
                     CI2CMaster *pI2CMaster = 0,   // 0 = no I2C DAC init
                     u8       ucI2CAddress = 0, ...);
```

For a PCM5102 every I2S-specific parameter takes its default: `bSlave = FALSE`
(Pi drives PCM/FS clocks — master), `pI2CMaster = 0` (no codec register init),
`nChunkSize = 8192`. So the construction mirrors the two-argument form of the
existing devices.

## Detailed Design

### 1. Device branch (`src/audio/audio_driver.{h,cpp}`)

- In `audio_driver.h`, add the include next to the existing sound-device headers:
  ```cpp
  #include <circle/sound/i2ssoundbasedevice.h>
  ```
- In `AudioDriver::Initialize()`, extend the device selection to three cases:
  ```cpp
  if (out == AudioOutput::Analog)
      m_pDevice = new CPWMSoundBaseDevice(m_pInterrupt, nSampleRate);
  else if (out == AudioOutput::I2S)
      m_pDevice = new CI2SSoundBaseDevice(m_pInterrupt, nSampleRate);
  else
      m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, nSampleRate);
  ```
  Everything after the construction (the null check, `AllocateQueue`,
  `SetWriteFormat`, `Start`, `IsActive`) is unchanged.

**Chunk-size caveat (hardware-verify item, not a code decision):** the default
`nChunkSize = 8192` is ≈93 ms/chunk at 44.1 kHz, which coexists with the 80 ms
`AllocateQueue` depth. Circle's write-queue buffering is independent of the DMA
chunk, so this is expected to be fine; if hardware shows choppiness, the remedy is
to pass a smaller explicit `nChunkSize` to the I2S constructor. We do **not**
pre-tune it blind — the hardware check decides.

### 2. Settings model (`src/settings/settings.{h,cpp}`)

- Extend the enum (`settings.h`):
  ```cpp
  enum class AudioOutput { HDMI, Analog, I2S };
  ```
- Parse (`parse_settings`, the `audio_output` branch): replace the binary
  ternary with a three-way decision; unknown/missing still falls back to HDMI:
  ```cpp
  else if (ieq(key, "audio_output"))
  {
      if      (ieq(val, "analog")) s.audio_output = AudioOutput::Analog;
      else if (ieq(val, "i2s"))    s.audio_output = AudioOutput::I2S;
      else                         s.audio_output = AudioOutput::HDMI;
  }
  ```
- Serialize helper (`audio_output_file_value`): add the I2S case:
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

The default remains `AudioOutput::HDMI` (the `Settings` constructor is unchanged).

### 3. Settings screen (`src/menu/settings_screen.cpp`)

The "Audio out" row is currently a 2-way toggle (case 7) with a binary display
value. Make both three-way:

- Display value (in `Render`):
  ```cpp
  const char *audioVal =
      m_pSettings->audio_output == AudioOutput::Analog ? "< Analog >"   :
      m_pSettings->audio_output == AudioOutput::I2S    ? "< I2S DAC >"  :
                                                         "< HDMI >";
  ```
- Edit handler (case 7 in the `dir` switch): cycle the three values, clamped-wrap
  like the Region row:
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

`Apply()` + `m_pStore->Save(*m_pSettings)` already run after the switch. The
"Audio: reboot." footer hint already covers the apply-on-reboot behavior, so no
text change is required.

## Error Handling

- Parse: unknown/missing `audio_output` → `HDMI` (unchanged default).
- Construction failure (allocation, `AllocateQueue`, or `Start` returning false):
  `Initialize()` returns `FALSE` as today; the kernel leaves `audioInited =
  FALSE` and runs silently — no crash. (Video and the menu remain usable, so the
  user can switch the output back.)
- A PCM5102 with no signal-sink connected cannot be detected (I2S is output-only,
  no readback); selecting I2S with no DAC attached yields silence, recoverable by
  switching the setting back. This is accepted, matching the analog path.

## Testing

**Host unit tests** (`test/test_settings.cpp`, extending the existing
`audio_output` block):
- `parse_settings("audio_output=i2s\n").audio_output == AudioOutput::I2S`.
- Existing assertions still hold: `analog → Analog`, `hdmi → HDMI`,
  `bogus → HDMI`.
- `audio_output_file_value(AudioOutput::I2S)` returns `"i2s"` (and the existing
  `hdmi`/`analog` cases).
- Round-trip: `Settings` with `audio_output = I2S`, `serialize_settings`,
  `parse_settings`, assert `I2S` survives.

(The device branch and the Settings row are Circle/hardware code with no host
tests, consistent with the rest of `audio_driver.cpp` / the menu screens. Build
verification is the repo-root `make` producing `kernel7.img`.)

**Hardware verification** (new checklist item, appended to the hardware-verify
checklist):
- Connect a PCM5102 I2S DAC HAT. Settings → Audio out = **I2S DAC** → reboot.
  **Expect:** game audio comes from the DAC; HDMI and the 3.5 mm jack are silent.
- With I2S selected, change volume and toggle mute. **Expect:** they scale /
  silence the I2S output the same as the other back ends.
- **Expect:** audio is clean — no choppiness/dropouts (the `nChunkSize` caveat); if
  choppy, note it for chunk-size tuning.
- Confirm `audio_output=i2s` in `SD:/settings.txt`; survives reboot.
- Set Audio out = HDMI, reboot → audio returns to HDMI; `audio_output=hdmi` in the
  file.

## Out of Scope (remains deferred)

- **I2C-configured DACs** (HifiBerry / PCM512x-class) that need register init —
  would require a `CI2CMaster` owned by the kernel plus the DAC's I2C address
  passed into the I2S constructor. A follow-on once a no-I2C board is proven.
- **Resampling / dynamic rate control** (deferred audio #4) — the PCM5102 accepts
  the core's 44.1 kHz directly.
- **Runtime (non-reboot) output switching** — the device is created once at boot,
  same as the existing analog/HDMI selection.

## Cross-references

- Audio deferred enhancements (#1) —
  `docs/superpowers/specs/2026-06-16-audio-output-deferred-enhancements.md`
- Analog (PWM) output, the sibling back end — [[project-analog-audio-output]]
- Volume/mute gain (applies to all back ends) — [[project-audio-settings]]
- FSD §4.5 (Audio Output), §4.9 (Configuration) —
  `Documents/Bare-Metal-Sega-Genesis-FSD.md`
</content>
