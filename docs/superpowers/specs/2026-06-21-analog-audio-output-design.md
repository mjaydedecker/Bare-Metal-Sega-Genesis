# Analog (3.5mm) Audio Output — Design

**Date:** 2026-06-21
**Status:** approved — ready for implementation plan
**Phase:** 3 (Polish and Expanded Support) — FSD §6 "Analog audio output"
**Supersedes the deferred note in:** `docs/superpowers/specs/2026-06-16-audio-output-deferred-enhancements.md` §1

## Goal

Let the user route emulator audio to the Raspberry Pi 2's **3.5mm headphone
jack** instead of HDMI. The output device is chosen in the Settings screen,
persisted to `SD:/settings.txt`, and applied on the **next boot**. The default
stays **HDMI**, so existing setups are unaffected.

This covers HDMI displays without speakers and analog/amp setups.

## Non-goals (stay deferred)

- I2S DAC HAT support (`CI2SSoundBaseDevice`) — a later addition.
- **Live** switching mid-session (tear down + re-create the running sound
  device). We apply on reboot, matching how `region` already behaves.
- Per-device latency / buffer-size configuration.

## Background

`AudioDriver` (`src/audio/audio_driver.{h,cpp}`) already hides the sound device
behind Circle's Write-queue API (`AllocateQueue` / `SetWriteFormat` / `Write` /
`GetQueueFramesAvail` / `Start` / `IsActive`). It currently holds a
`CHDMISoundBaseDevice*`.

On the Pi 2 (`RASPPI=2`) the 3.5mm jack is driven by `CPWMSoundBaseDevice`
(`circle/sound/pwmsoundbasedevice.h`), which derives from the same
`CSoundBaseDevice` base as the HDMI device and exposes the identical queue API.
Volume and mute already live in `AudioDriver::Write()` and are device-agnostic,
so they carry over to the PWM path unchanged.

The whole feature mirrors the existing **`region`** setting end-to-end: enum →
parse → serialize → settings-screen cycle row → applied at boot.

## Design

### Settings model — `src/settings/settings.h` / `settings.cpp`

- New `enum class AudioOutput { HDMI, Analog };`
- New `Settings` field `AudioOutput audio_output;`, default `AudioOutput::HDMI`
  (set in the constructor alongside the other defaults).
- Parse: key `audio_output`, value `hdmi` | `analog`; any unknown value defaults
  to `hdmi` (same forgiving pattern as `region`/`video_mode`).
- Serialize: append `\naudio_output=<value>` in the settings writer.
- Helper `const char *audio_output_file_value(AudioOutput o);` returning
  `"hdmi"` / `"analog"`.

### Audio driver — `src/audio/audio_driver.{h,cpp}`

- Member type changes from `CHDMISoundBaseDevice *m_pDevice` to
  `CSoundBaseDevice *m_pDevice` (the common base). Include both
  `hdmisoundbasedevice.h` and `pwmsoundbasedevice.h`.
- `Initialize(unsigned nSampleRate)` → `Initialize(unsigned nSampleRate,
  AudioOutput out)`. Based on `out`, `new CHDMISoundBaseDevice(...)` or
  `new CPWMSoundBaseDevice(m_pInterrupt, nSampleRate)`. Everything after the
  allocation (`AllocateQueue`, `SetWriteFormat`, `Start`, `IsActive`) is
  unchanged and shared.
- No other method changes — `Write`, `QueuedFrames`, `IsReady`, volume/mute,
  and the underrun/overrun metrics are untouched.

### Kernel wiring — `src/kernel.cpp`

- The single `m_Audio.Initialize(sampleRate)` call passes
  `m_Settings.audio_output`. No other change; the device is created once at
  startup as it is today.

### Settings screen — `src/menu/settings_screen.cpp`

- Add an **"Audio out:"** cycle row showing `< HDMI >` / `< Analog >`, cycling
  between the two values (same handler shape as the `region` row).
- Extend the existing "applies on …" footnote to note that **Audio out** takes
  effect on **reboot** (region already documents "on reload" there).

## Error handling

If the selected device fails to initialize (`new`, `AllocateQueue`, or `Start`
fails), `Initialize()` returns `FALSE` exactly as today. The kernel leaves
`audioInited` false and the game runs **silently** rather than crashing. There
is **no** automatic fallback to HDMI: a failed analog init should not silently
misreport which output is active — the user sees no sound and can switch back.

## Testing

- **Host unit test** (new `test/test_audio_output.cpp`, following the
  `test/test_settings` pattern, registered in `test/Makefile` and `.gitignore`):
  - `audio_output=analog` parses to `AudioOutput::Analog`; `hdmi` to `HDMI`.
  - Unknown / missing value defaults to `HDMI`.
  - Round-trip: serialize → parse preserves the value.
  - `audio_output_file_value()` returns the expected strings.
- **Device instantiation** can't run host-side (needs Pi hardware); add a row to
  the next hardware-verification checklist:
  - Set Audio out = Analog, reboot, confirm sound comes from the 3.5mm jack and
    not HDMI; volume/mute still work; switch back to HDMI restores HDMI audio.

## Files touched

- `src/settings/settings.h`, `src/settings/settings.cpp`
- `src/audio/audio_driver.h`, `src/audio/audio_driver.cpp`
- `src/kernel.cpp`
- `src/menu/settings_screen.cpp`
- `test/test_audio_output.cpp` (new), `test/Makefile`, `.gitignore`
