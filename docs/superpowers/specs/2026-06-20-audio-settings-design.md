# Audio Settings — Design (volume/mute + metrics)

**Date:** 2026-06-20
**Status:** approved design, pending implementation plan
**Related:** FSD §4.5 / §4.9, `docs/superpowers/specs/2026-06-16-audio-output-deferred-enhancements.md` (items #2 volume, #5 metrics), `docs/superpowers/specs/2026-06-20-video-settings-design.md` (the settings subsystem this builds on)

## Summary

Add user-facing **master volume** and **mute** to the emulator, plus internal
**underrun/overrun audio-queue metrics** for tuning. Volume/mute reuse the
settings subsystem (`SD:/settings.txt` + the in-emulation Settings screen) added
for video settings. Metrics are a developer/tuning aid logged over serial.

Output-device selection (HDMI vs 3.5mm analog) is **explicitly out of scope** —
deferred to a follow-on spec (it needs a live device-reconfigure path and analog
hardware to verify).

## Key Constraint (drives the whole design)

The frame loop is paced by the audio queue: each frame writes audio, and the
loop gates on `AudioDriver::QueuedFrames()`. **Audio must be written every frame
to advance the queue.** Therefore **mute writes silence (zeroed samples), never
skips the write**, and volume scales sample *values* while always writing the
same frame count. Skipping writes would starve the queue and break pacing — the
exact failure mode recorded in the perf-ceiling history.

## Architecture

### Single choke point: `AudioDriver::Write()`

Both `audio_sample_cb` and `audio_batch_cb` already funnel samples through
`AudioDriver::Write(const s16*, unsigned nFrames)`. Volume/mute is applied there:

- **Fast path** — when `volume == 100 && !mute`, write the core's buffer directly
  (zero-copy; identical to today's behavior).
- **Scaled path** — otherwise, copy samples into a small static staging buffer,
  scaling each (or writing zeros when muted), and write to the device in chunks
  (e.g. 1024 frames per chunk). The caller is single-threaded (main loop), and
  the device's DMA/IRQ reads the device queue, not our staging buffer, so the
  static staging buffer is safe.

### Pure, host-tested helpers: `src/audio/audio_util.{h,cpp}`

No Circle dependencies, unit-tested like `blit`/`settings`:

- `s16 scale_sample(s16 sample, unsigned volume, bool mute);`
  - `mute` → 0; `volume == 0` → 0; `volume == 100` → unchanged;
    otherwise `(s32)sample * volume / 100`. Scaling down cannot overflow s16.
- `enum AudioQueueEvent { AQ_None, AQ_Underrun, AQ_Overrun };`
  `AudioQueueEvent classify_queue(unsigned frames, unsigned low, unsigned high);`
  - `frames <= low` → `AQ_Underrun`; `frames > high` → `AQ_Overrun`;
    else `AQ_None`.

### Metrics counters in `AudioDriver`

`AudioDriver` holds `m_Underruns` / `m_Overruns` with `RecordUnderrun()`,
`RecordOverrun()`, `Underruns()`, `Overruns()`. The kernel's pacing loop already
reads `QueuedFrames()`; it calls `classify_queue(frames, low, high)` once per
frame — with `low` near 0 and `high = target + framesPerVideo` (the existing
high-watermark) — and bumps the matching counter.

## Components & Data Flow

### Settings model (`src/settings/settings.{h,cpp}`)

Add two fields to `Settings`:

| Field | Type | Default | File key |
|---|---|---|---|
| `volume` | `unsigned` (0–100) | `100` | `volume=100` |
| `mute` | `bool` | `false` | `mute=off` |

`parse_settings` clamps `volume` to [0,100] (non-numeric → default 100); `mute`
parses with the existing truthy logic (unknown → false). `serialize_settings`
appends both keys. Parser tolerates any 0–100 value; the screen adjusts in steps
of 10.

### AudioDriver (`src/audio/audio_driver.{h,cpp}`)

- Add `m_Volume` (default 100), `m_Mute` (default false), `SetVolume(unsigned)`,
  `SetMute(bool)`. Both are safe before `Initialize()` — they only store state,
  applied when `Write()` runs (audio initializes lazily on first ROM load).
- Apply gain in `Write()` per the choke-point design (uses `scale_sample`).
- Add metrics counters + accessors (above).

### Settings screen (`src/menu/settings_screen.{h,cpp}`)

Grows from 2 rows to 4:

```
Settings

  Video Scale:  < Integer | Stretch >
  Widescreen:   < Off | On >   (applies on reset)
  Volume:       < 100 >
  Mute:         < Off >

  B: back
```

- **Volume** row: left/right adjusts ±10, clamped to [0,100].
- **Mute** row: left/right toggles.
- `SettingsScreen` gains an `AudioDriver*`; `Apply()` adds
  `m_pAudio->SetVolume(...)` and `m_pAudio->SetMute(...)` (live), alongside the
  existing scale-mode/widescreen apply. Each change is persisted via
  `SettingsStore::Save` as today.

### Kernel (`src/kernel.{h,cpp}`)

- Construct `SettingsScreen` with `&m_Audio` added.
- At boot (where it already applies scale mode / widescreen), also call
  `m_Audio.SetVolume(m_Settings.volume)` and `m_Audio.SetMute(m_Settings.mute)`.
- In the play loop, after the existing audio gate, call `classify_queue` on the
  current `QueuedFrames()` and bump the `AudioDriver` counters.
- Periodically (~every 300 frames / 5 s) log
  `audio underruns=N overruns=M` via `CLogger` (`LogNotice`).

### Metrics surfacing — honest caveat

The logger target is `m_Options.GetLogDevice()`, which defaults to the console
screen (hidden behind the game framebuffer during play) and only reaches the
UART when `logdev=ttyS1` is set on the kernel command line. So metrics are a
**developer/tuning aid visible over serial when so configured**, not an on-screen
feature. This is intentional ("instrument first, expose later") and avoids
per-frame overlay cost. A future latency setting, if justified by these numbers,
would be its own change.

## Error Handling

- Volume parse: non-numeric / out-of-range → clamp to [0,100]; `mute` unknown →
  default off.
- `SetVolume` / `SetMute` before audio init: stored; no-op until `Write` runs.
- Best-effort persistence: a `SettingsStore::Save` failure keeps the in-session
  change and does not crash (consistent with the rest of settings).
- Mute / `volume == 0`: writes silence of the correct frame count — pacing
  unaffected.

## Testing

**Host unit tests** (`test/`, existing pattern):
- `scale_sample`: mute → 0; volume 0 → 0; volume 100 → identity (incl. negative
  samples); volume 50 → half; boundary values (e.g. -32768, 32767).
- `classify_queue`: `frames == 0` and `frames == low` → Underrun;
  `frames > high` → Overrun; mid-range → None.
- `settings`: `volume`/`mute` round-trip; clamping of out-of-range and
  non-numeric volume; `mute` truthy parsing; defaults when keys absent.

**Hardware verification:**
- Volume up/down audibly changes loudness in-game.
- Mute silences output **and playback stays smooth** (no garble/desync) —
  proving silence-write keeps pacing intact; unmute restores sound.
- With `logdev=ttyS1`, the serial log shows underrun/overrun counts staying
  steady (not climbing) during normal play.
- Reboot: `volume`/`mute` persisted in `SD:/settings.txt`.

## Out of Scope (future specs)

- HDMI ↔ analog (PWM) / I2S output-device selection (FSD `audio_output`).
- User-facing audio latency setting (revisit only if metrics justify it).
- Resampling / dynamic rate control; GetChunk pull driver model.
