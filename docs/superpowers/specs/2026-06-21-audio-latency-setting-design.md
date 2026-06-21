# Configurable Audio Latency Setting — Design

**Date:** 2026-06-21
**Status:** approved design, pending implementation plan
**Related:** audio deferred-enhancements #3 (config-driven buffer/latency) and #5
(latency tuning — metrics half already shipped);
`docs/superpowers/specs/2026-06-16-audio-output-deferred-enhancements.md`;
the settings subsystem + Settings screen; [[project-audio-settings]];
[[project-genesis-perf-ceiling]] (the proven timer-pacing + high-watermark gate
this setting parameterizes).

## Summary

Expose the audio buffering latency as a user setting — a 3-way preset
(`audio_latency = low | medium | high`) chosen on the Settings screen, stored in
`SD:/settings.txt`, and applied **live**. It scales how many video-frames of
audio the pacing loop lets build up before the high-watermark gate holds,
trading input-to-sound latency against underrun safety. The default (`medium`)
reproduces today's exact pacing numbers, so existing behavior is unchanged unless
the user opts in.

This closes deferred audio enhancement #3's buffer/latency sub-part (the
output-device sub-part already shipped as the `audio_output` setting) and the
tunable half of #5 (the underrun/overrun metrics already shipped). Resampling and
dynamic rate control (#4) remain deferred.

## Background / Constraint

The play loop in `src/kernel.cpp` paces frames with a timer plus a hard
high-watermark busy-wait gate (the model that resolved the perf ceiling —
[[project-genesis-perf-ceiling]]; do not replace it). The relevant code, computed
once per ROM load:

```cpp
unsigned framesPerVideo = sampleRate ? (unsigned) (sampleRate / fps) : 0;  // ~735 @44.1k/60
unsigned target         = framesPerVideo * 2;                              // ~33 ms buffered
```

and per frame, after the timer wait:

```cpp
switch (classify_queue (q, 0, target + framesPerVideo)) { ... }   // metrics
while (m_Audio.QueuedFrames () > target + framesPerVideo) { }      // the gate
```

So the **effective steady-state latency floats between `target` (low-water) and
`target + framesPerVideo` (high-water / gate)**. The device queue is allocated
once at a fixed `AudioDriver::QUEUE_MS = 80` ceiling, which the gate never reaches.

The only knob a user perceives as "latency" is `target` (and the gate one frame
above it). This design parameterizes `target` by a preset multiplier; everything
else about the audio path is unchanged.

## Model & Mapping

The preset selects the `target` multiplier in **video-frames** of buffered audio.
The watermark formula (`target + framesPerVideo`) is unchanged, so the gate sits
one frame above the target in every preset.

| Preset            | target           | gate (high-water)    | approx latency @60 fps |
|-------------------|------------------|----------------------|------------------------|
| Low               | 1 × framesPerVideo | 2 × framesPerVideo | ~17–33 ms              |
| **Medium (default)** | 2 × framesPerVideo | 3 × framesPerVideo | ~33–50 ms (= today) |
| High              | 3 × framesPerVideo | 4 × framesPerVideo | ~50–67 ms              |

High's gate (4 frames ≈ 67 ms @44.1k/60) stays under the existing 80 ms device
queue allocation, so `AudioDriver::QUEUE_MS` and `AudioDriver::Initialize()` are
**not touched**. Medium's multiplier of 2 reproduces today's `target =
framesPerVideo * 2` exactly — the default is a no-op change.

## Settings Model (`src/settings/settings.{h,cpp}`)

- New enum (in `settings.h`, with the other setting enums):
  ```cpp
  enum class AudioLatency { Low, Medium, High };
  ```
- New `Settings` field: `AudioLatency audio_latency;`, initialized to
  `AudioLatency::Medium` in the constructor's initializer list.
- File key `audio_latency`, values `low | medium | high`. Parse in
  `parse_settings` (mirroring the `region` / `menu_hotkey` branches):
  ```cpp
  else if (ieq(key, "audio_latency"))
  {
      if      (ieq(val, "low"))  s.audio_latency = AudioLatency::Low;
      else if (ieq(val, "high")) s.audio_latency = AudioLatency::High;
      else                       s.audio_latency = AudioLatency::Medium;
  }
  ```
- `serialize_settings` writes the key in the audio group (after `mute`, near the
  other audio keys):
  ```cpp
  appendz(out, out_size, "\naudio_latency=");
  appendz(out, out_size, audio_latency_file_value(s.audio_latency));
  ```
- Pure helpers (declared in `settings.h`, defined in `settings.cpp` next to
  `region_file_value` / `menu_hotkey_file_value`):
  ```cpp
  // AudioLatency as written to the settings file ("low" | "medium" | "high").
  const char *audio_latency_file_value(AudioLatency l);

  // Target buffered-audio depth as a video-frame multiplier (Low=1, Medium=2,
  // High=3); kernel computes target = audio_latency_frames(l) * framesPerVideo.
  unsigned audio_latency_frames(AudioLatency l);
  ```
  with:
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

Keeping both helpers in `settings.cpp` means `src/audio/audio_util.*` and the
Circle audio device code stay untouched; the kernel does the only multiplication.

## Pacing Wiring (`src/kernel.cpp`)

- Remove the per-ROM-load `unsigned target = framesPerVideo * 2;` (line ~248).
- Inside the play `for (;;)` loop, compute `target` live each frame from the live
  setting (the same live-read pattern the menu hotkey already uses at line ~288),
  before it is consumed by the HUD and the gate:
  ```cpp
  unsigned target = audio_latency_frames (m_Settings.audio_latency) * framesPerVideo;
  ```
- The watermark expressions are unchanged and continue to read `target`:
  ```cpp
  classify_queue (q, 0, target + framesPerVideo);
  while (m_Audio.QueuedFrames () > target + framesPerVideo) { }
  ```
- The HUD line already publishes `st.target = target;`, so it now reflects the
  live latency with no extra code.

A mid-play change causes one brief, harmless queue re-settle (a momentary
underrun when lowering, a momentary fuller queue when raising). No audible glitch
beyond that transient; no reset required.

## Settings Screen (`src/menu/settings_screen.{h,cpp}`)

Add one cycle row, `Audio Latency: < Low | Medium | High >`, following the exact
pattern of the existing Menu Hotkey row (`settings_screen.cpp:202-207`):

- Display label derived from `m_pSettings->audio_latency`
  (`"< Low >"` / `"< Medium >"` / `"< High >"`).
- Left/Right cycles the enum, clamped to `[Low, High]`:
  ```cpp
  int l = (int) m_pSettings->audio_latency + dir;
  if (l < (int) AudioLatency::Low)  l = (int) AudioLatency::Low;
  if (l > (int) AudioLatency::High) l = (int) AudioLatency::High;
  m_pSettings->audio_latency = (AudioLatency) l;
  ```
- Persist via `m_pStore->Save(*m_pSettings)` like the other rows (the screen
  holds the store as `m_pStore`). **No
  `Apply()` call** — the kernel reads the value live each frame.

The new row is added to the screen's row list/labels and its `case` in the
edit-handling switch, following the established indexing in `settings_screen.cpp`.
(The implementation plan resolves the exact row index against the current list.)

## Error Handling

- Parse: unknown / missing `audio_latency` → `AudioLatency::Medium` (safe default
  = today's behavior).
- The enum is always one of three valid values; `audio_latency_frames` returns a
  valid multiplier for every case. No invalid latency is representable.
- `framesPerVideo == 0` (no sample rate) path is unchanged — audio is not
  initialized, so the pacing gate's audio branch is skipped as today.

## Testing

**Host unit tests** (`test/test_settings.cpp`, extended):
- Default: `Settings().audio_latency == AudioLatency::Medium`.
- Parse each keyword → the right enum
  (`low`→Low, `medium`→Medium, `high`→High).
- Parse invalid / missing → `Medium`
  (`parse_settings("audio_latency=bogus\n")` and a text without the key).
- `audio_latency_frames`: Low→1, Medium→2, High→3.
- `audio_latency_file_value`: each enum → the right keyword.
- Round-trip: set each value, `serialize_settings`, `parse_settings`, assert the
  value survives.

(The kernel pacing change and the Settings-screen row are exercised on hardware,
consistent with the rest of `kernel.cpp` / the menu screens, which have no host
tests.)

**Hardware verification** (new checklist item, appended to the hardware-verify
checklist):
- Set Audio Latency = Low → resume → audio still clean on a demanding game; no
  constant underruns climbing in the ~5 s log / HUD U counter.
- Set = High → audio remains clean; HUD `target` value increases accordingly.
- Set = Medium → behaves exactly as before this feature.
- Confirm `audio_latency` is written to `SD:/settings.txt` and survives a reboot.

## Out of Scope (remains deferred)

- Resampling and dynamic rate control (deferred audio #4) — this setting does not
  touch the busy-wait pacing model, only its target depth.
- Per-output-device latency differences (HDMI vs PWM).
- A numeric milliseconds / frames control (preset only, matching the other
  settings).
- Changing the device queue allocation (`QUEUE_MS`) — fixed 80 ms covers all
  three presets.

## Cross-references

- Audio deferred enhancements (#3, #5) —
  `docs/superpowers/specs/2026-06-16-audio-output-deferred-enhancements.md`
- Audio settings (volume/mute/metrics) — [[project-audio-settings]]
- Perf model this parameterizes — [[project-genesis-perf-ceiling]]
- FSD §4.5 (Audio Output), §4.9 (Configuration) —
  `Documents/Bare-Metal-Sega-Genesis-FSD.md`
</content>
</invoke>
