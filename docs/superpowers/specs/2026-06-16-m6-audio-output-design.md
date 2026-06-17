# M6 — Audio Output: Design

**Date:** 2026-06-16
**Milestone:** M6 (Audio Output)
**Status:** design approved; implementation plan to follow.
**Target hardware:** Raspberry Pi 2 (`RASPPI=2`, AArch32, `kernel7.img`).

## Goal

Make the emulated Genesis audio audible through the TV (HDMI) at the correct
speed, by wiring the libretro audio callbacks to a Circle sound device and making
the audio hardware the frame-pacing clock. This proves end-to-end audio and
retroactively fixes M5's approximate (timer-based) video speed.

**Done when:** game music and SFX play through the TV with no persistent crackle
or dropouts; the loop is paced by the audio clock so the game runs at correct
speed with A/V in sync; both libretro audio callbacks (batch + single-sample) are
handled; sustained play (minutes) shows no progressive drift or underrun buildup.

## Decisions (and the alternatives we rejected)

| Decision | Choice | Rejected alternative (deferred) |
|----------|--------|---------------------------------|
| Output device | **HDMI** (`CHDMISoundBaseDevice`) — sound via the TV | PWM analog 3.5mm jack; I2S external DAC — backlog #1 |
| Frame pacing | **Audio-driven** — audio queue is the clock | Keep M5 timer pacing (drift/underruns) |
| Driver model | **Write-queue** (push from callbacks) | `GetChunk` pull/generator model — backlog #6 |
| Sample format | Core's **signed-16 stereo**, fed directly | Resampling / format conversion — backlog #4 |
| Scope | **Minimal**: audible + audio-paced | Volume / config / latency UI — backlog #2, #3 |

The core emits **interleaved signed-16 stereo**, which maps directly to
`SetWriteFormat(SoundFormatSigned16, 2)` — no conversion (the same lucky break as
RGB565 video). Sample rate comes from `retro_get_system_av_info()`
(`avInfo.timing.sample_rate`, ~44100 for Genesis-Plus-GX).

## Architecture

```
CKernel ──owns──> AudioDriver ──owns──> CHDMISoundBaseDevice (HDMI, signed-16 stereo)
   │
   └──sets──> g_audio ──used by──> audio_batch_cb / audio_sample_cb ──> AudioDriver::Write()
```

- **`AudioDriver`** (`src/audio/audio_driver.{h,cpp}`) — owns the sound device;
  knows nothing about libretro. Mirrors how `Display` wraps the framebuffer.
- **`audio_batch_cb` / `audio_sample_cb`** (`src/libretro/callbacks.cpp`) —
  forward samples to the `AudioDriver` via a `g_audio` global (same pattern as
  `g_display`).
- **`CKernel`** — owns `AudioDriver m_Audio`, initializes it with the core's
  sample rate, sets `g_audio`, and runs the audio-paced loop.

Dependency direction: `kernel → audio_driver`, `kernel → callbacks → (g_audio) →
audio_driver`. `AudioDriver` depends only on Circle's `CHDMISoundBaseDevice`.

## Components

### `AudioDriver` (`src/audio/audio_driver.h` / `.cpp`)

```cpp
class AudioDriver
{
public:
    static const unsigned QUEUE_MS = 80;   // queue depth in milliseconds

    AudioDriver(CInterruptSystem *pInterrupt);
    ~AudioDriver(void);

    boolean Initialize(unsigned nSampleRate);          // alloc queue, start device
    void     Write(const s16 *pSamples, unsigned nFrames);  // push stereo frames
    unsigned QueuedFrames(void);                       // frames waiting to play
    boolean  IsReady(void) const;                      // device active

private:
    CInterruptSystem      *m_pInterrupt;
    CHDMISoundBaseDevice  *m_pDevice;
};
```

- `Initialize(rate)`: `m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, rate)`;
  `AllocateQueue(QUEUE_MS)`; `SetWriteFormat(SoundFormatSigned16, 2)`; `Start()`;
  return `m_pDevice->IsActive()`. On any failure, delete and return FALSE.
- `Write(samples, frames)`: `m_pDevice->Write(samples, frames * 4)` (4 bytes per
  signed-16 stereo frame). Return value (bytes consumed) is ignored — pacing
  guarantees room; a rare short write drops the remainder.
- `QueuedFrames()`: `m_pDevice->GetQueueFramesAvail()` (frames queued, not yet
  played) — the pacing signal.
- `IsReady()`: `m_pDevice != 0 && m_pDevice->IsActive()`.

### `audio_batch_cb` / `audio_sample_cb` (`src/libretro/callbacks.cpp`)

```cpp
AudioDriver *g_audio = 0;

size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    if (g_audio != 0) g_audio->Write(data, (unsigned) frames);
    return frames;
}

void audio_sample_cb(int16_t left, int16_t right)
{
    if (g_audio != 0)
    {
        int16_t frame[2] = { left, right };
        g_audio->Write(frame, 1);
    }
}
```

`g_audio` is forward-declared in `callbacks.h` (`class AudioDriver; extern
AudioDriver *g_audio;`), same as `g_display`.

### `CKernel` changes (`src/kernel.{h,cpp}`)

- Replace the unused `CPWMSoundDevice m_Sound` member with `AudioDriver m_Audio`;
  swap the `<circle/sound/pwmsounddevice.h>` include for `"audio/audio_driver.h"`;
  construct `m_Audio(&m_Interrupt)` (replacing the `m_Sound(&m_Interrupt)` init).
- In `Run()`: after AV-info, initialize audio and enter the audio-paced loop.

## Data flow — per frame

1. `retro_run()` advances emulation one frame.
2. The core calls `audio_batch_cb(data, frames)` (one or more times) →
   `g_audio->Write()` → samples enter the HDMI device queue. DMA drains the queue
   at `sampleRate` → TV speakers.
3. The core calls `video_refresh_cb` → `Display::Blit` (unchanged from M5).
4. The loop waits until queued audio drains below `target` before the next frame.

## Frame loop & pacing (replaces the M5 timer loop)

```cpp
unsigned sampleRate     = (unsigned) avInfo.timing.sample_rate;     // ~44100
double   fps            = avInfo.timing.fps; if (fps < 1.0) fps = 60.0;
unsigned framesPerVideo = (unsigned) (sampleRate / fps);            // ~735
unsigned target         = framesPerVideo * 2;                       // ~2 frames

bool audioOK = (sampleRate > 0) && m_Audio.Initialize(sampleRate);
if (!audioOK)
    m_Logger.Write(FromKernel, LogWarning, "Audio disabled; using timer pacing");
g_audio = audioOK ? &m_Audio : 0;

u64 next = CTimer::GetClockTicks64();
unsigned period_us = (unsigned) (1000000.0 / fps);
for (;;)
{
    retro_run();                                   // fills audio queue + blits video

    if (audioOK)
    {
        while (m_Audio.QueuedFrames() > target) { /* pace to audio clock */ }
    }
    else
    {
        next += period_us;                          // M5 timer fallback
        u64 now = CTimer::GetClockTicks64();
        if (next < now) next = now;                 // running behind: drop slack
        while (CTimer::GetClockTicks64() < next) { } // spin to deadline
    }
    // periodic ACT-LED liveness toggle
}
```

- **Audio path:** the HDMI device consumes at the true sample rate; blocking until
  the queue drains below `target` makes audio the clock and video rides along.
- **Self-priming:** the queue starts empty, so the first 1–2 iterations run
  without waiting until ~`target` frames accumulate; then steady-state pacing.
- **Sizing:** `QUEUE_MS=80` (~3500 frames at 44.1 kHz) with `target` ~2 video
  frames (~33 ms) leaves headroom; both tunable on hardware.
- Input callbacks remain no-op stubs (M7).

## Error handling

- **Audio init failure** (`sampleRate==0`, device alloc, or `Start()` fails) →
  log `LogWarning` to serial and **continue video-only** with the M5 timer-paced
  fallback (`g_audio` stays null). A working video system is never lost to an
  audio fault.
- **Null/inactive guard:** the audio callbacks no-op when `g_audio` is null, so a
  fallback run does not crash.
- **`audio_sample_cb`:** Genesis-Plus-GX uses the batch path almost exclusively;
  the single-sample path is still handled (pack one frame, `Write(...,1)`).
- **Partial `Write`:** with pacing the queue has room; a rare short write drops the
  remainder (momentary, self-correcting). No blocking inside the callback.
- No allocation in the hot path — the queue is allocated once in `Initialize()`.

## Testing

Unlike M5's blit, M6 is thin glue over a Circle device plus hardware behavior; the
only pure logic is the two-line pacing-target math. Extracting it just to test it
would be over-engineering.

- **No new host unit tests.** Pacing math stays inline in `Run()` and is verified
  by reading.
- **Regression guard:** the existing `test/` blit suite must still pass (M6 touches
  the kernel loop and callbacks).
- **Build gate:** clean cross-compile of `kernel7.img`.

### On-hardware acceptance

- Sonic demo: music **and** SFX audible through the TV, no persistent crackle or
  dropouts.
- Correct speed and **A/V in sync** (audio is the clock — also confirms M5 speed).
- Sustained play (minutes) with no progressive drift or underrun buildup.
- Fallback: if HDMI audio fails to init, serial shows "Audio disabled" and **video
  still runs** (timer-paced).

## Files

- `src/audio/audio_driver.h` — new (`AudioDriver` class).
- `src/audio/audio_driver.cpp` — new.
- `src/libretro/callbacks.cpp` / `.h` — implement audio callbacks, add `g_audio`.
- `src/kernel.h` / `kernel.cpp` — `m_Audio` member (replaces `m_Sound`), init,
  audio-paced loop with timer fallback.
- `Makefile` — add `src/audio/audio_driver.o` to `OBJS` and `EXTRACLEAN`.

## Deferred work

Higher-order alternatives are recorded in
[`2026-06-16-audio-output-deferred-enhancements.md`](2026-06-16-audio-output-deferred-enhancements.md):
alternate outputs (PWM/I2S/selectable) (#1), volume control (#2), config-driven
audio settings (#3), resampling / dynamic rate control (#4), latency tuning &
metrics (#5), the `GetChunk` pull model (#6).
