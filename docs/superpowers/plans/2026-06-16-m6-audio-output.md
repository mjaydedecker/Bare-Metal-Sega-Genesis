# M6 Audio Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the emulated Genesis audio play through the TV (HDMI) at correct speed by wiring libretro's audio callbacks to a Circle sound device and making the audio queue the frame-pacing clock.

**Architecture:** An `AudioDriver` class owns a `CHDMISoundBaseDevice` and exposes a tiny Write/queue interface. The core's signed-16 stereo samples are fed directly (no conversion) from `audio_batch_cb`/`audio_sample_cb` via a `g_audio` global. The kernel's frame loop paces on the audio queue level (`GetQueueFramesAvail()`), replacing M5's timer loop, and falls back to timer pacing (video-only) if HDMI audio fails to initialize.

**Tech Stack:** C++, bare-metal Circle (Raspberry Pi 2 / AArch32), Genesis-Plus-GX-Wide libretro core. No new host tests (thin pure surface); the existing `test/` blit suite is the regression guard.

**Reference spec:** `docs/superpowers/specs/2026-06-16-m6-audio-output-design.md`
**Deferred work:** `docs/superpowers/specs/2026-06-16-audio-output-deferred-enhancements.md`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `src/audio/audio_driver.h` / `.cpp` (new) | `AudioDriver` — owns `CHDMISoundBaseDevice`; Initialize / Write / QueuedFrames / IsReady. |
| `src/libretro/callbacks.{h,cpp}` (modify) | Implement `audio_batch_cb`/`audio_sample_cb`; add `g_audio`. |
| `src/kernel.{h,cpp}` (modify) | Replace dead `m_Sound` with `m_Audio`; init audio; audio-paced loop + timer fallback. |
| `Makefile` (modify) | Add `src/audio/audio_driver.o` to `OBJS` and `EXTRACLEAN`. |
| `docs/m6-hardware-setup.md` (new) | On-hardware acceptance checklist. |

---

## Task 1: `AudioDriver` class

**Files:**
- Create: `src/audio/audio_driver.h`
- Create: `src/audio/audio_driver.cpp`
- Modify: `Makefile` (`OBJS`, `EXTRACLEAN`)

- [ ] **Step 1: Write the AudioDriver header**

Create `src/audio/audio_driver.h`:

```cpp
//
// src/audio/audio_driver.h
//
// Bare Metal Sega Genesis
// Owns an HDMI sound device and feeds it the core's signed-16 stereo samples
// via Circle's Write queue. Knows nothing about libretro.
//

#ifndef _audio_audio_driver_h
#define _audio_audio_driver_h

#include <circle/interrupt.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/types.h>

class AudioDriver
{
public:
    static const unsigned QUEUE_MS = 80;   // queue depth in milliseconds

    AudioDriver(CInterruptSystem *pInterrupt);
    ~AudioDriver(void);

    // Allocate the queue and start the HDMI device at nSampleRate.
    boolean Initialize(unsigned nSampleRate);

    // Push nFrames of interleaved signed-16 stereo samples.
    void Write(const s16 *pSamples, unsigned nFrames);

    // Frames queued but not yet played (the pacing signal).
    unsigned QueuedFrames(void);

    boolean IsReady(void) const;

private:
    CInterruptSystem     *m_pInterrupt;
    CHDMISoundBaseDevice *m_pDevice;
};

#endif
```

- [ ] **Step 2: Write the AudioDriver implementation**

Create `src/audio/audio_driver.cpp`:

```cpp
//
// src/audio/audio_driver.cpp
//
// Bare Metal Sega Genesis
// See audio_driver.h.
//

#include "audio_driver.h"
#include <circle/sound/soundbasedevice.h>

AudioDriver::AudioDriver(CInterruptSystem *pInterrupt)
:   m_pInterrupt(pInterrupt), m_pDevice(0)
{
}

AudioDriver::~AudioDriver(void)
{
    delete m_pDevice;
    m_pDevice = 0;
}

boolean AudioDriver::Initialize(unsigned nSampleRate)
{
    if (nSampleRate == 0)
    {
        return FALSE;
    }

    m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, nSampleRate);
    if (m_pDevice == 0)
    {
        return FALSE;
    }

    if (!m_pDevice->AllocateQueue(QUEUE_MS))
    {
        delete m_pDevice;
        m_pDevice = 0;
        return FALSE;
    }

    m_pDevice->SetWriteFormat(SoundFormatSigned16, 2);

    if (!m_pDevice->Start())
    {
        delete m_pDevice;
        m_pDevice = 0;
        return FALSE;
    }

    return m_pDevice->IsActive();
}

void AudioDriver::Write(const s16 *pSamples, unsigned nFrames)
{
    if (m_pDevice != 0 && nFrames > 0)
    {
        // 4 bytes per signed-16 stereo frame; Write() takes a byte count.
        m_pDevice->Write(pSamples, (size_t) nFrames * 4);
    }
}

unsigned AudioDriver::QueuedFrames(void)
{
    return m_pDevice != 0 ? m_pDevice->GetQueueFramesAvail() : 0;
}

boolean AudioDriver::IsReady(void) const
{
    return m_pDevice != 0 && m_pDevice->IsActive();
}
```

- [ ] **Step 3: Add the object to the Makefile**

In `Makefile`, add the audio object to `OBJS` (currently ends with the two video objects):

```makefile
       src/video/blit.o \
       src/video/display.o \
       src/audio/audio_driver.o
```

And extend `EXTRACLEAN` to include the audio directory:

```makefile
EXTRACLEAN = src/*.o src/*.d src/storage/*.o src/storage/*.d \
             src/libretro/*.o src/libretro/*.d \
             src/video/*.o src/video/*.d \
             src/audio/*.o src/audio/*.d \
             build/genesis libs/libgenesis.a
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: compiles `src/audio/audio_driver.o`, links, ends with `COPY  kernel7.img`. No errors.

- [ ] **Step 5: Commit**

```bash
git add src/audio/audio_driver.h src/audio/audio_driver.cpp Makefile
git commit -m "M6: add AudioDriver wrapping CHDMISoundBaseDevice

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Wire the audio callbacks

**Files:**
- Modify: `src/libretro/callbacks.h`
- Modify: `src/libretro/callbacks.cpp`

- [ ] **Step 1: Declare `g_audio` in the callbacks header**

In `src/libretro/callbacks.h`, find the existing video global block:

```cpp
// Set by the kernel before the frame loop; video_refresh_cb forwards
// frames here. Forward-declared to avoid pulling Circle into this header.
class Display;
extern Display *g_display;
```

Add the audio global right after it:

```cpp
// Set by the kernel before the frame loop; the audio callbacks forward
// samples here. Forward-declared to avoid pulling Circle into this header.
class AudioDriver;
extern AudioDriver *g_audio;
```

- [ ] **Step 2: Define `g_audio` and implement the audio callbacks**

In `src/libretro/callbacks.cpp`, add the include after the existing display include (line 12, `#include "../video/display.h"`):

```cpp
#include "../audio/audio_driver.h"
```

Add the definition after the existing `Display *g_display = 0;` line:

```cpp
AudioDriver *g_audio = 0;
```

Replace the two no-op audio stubs:

```cpp
void audio_sample_cb(int16_t left, int16_t right)
{
    (void)left; (void)right;
}

size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    (void)data;
    return frames;
}
```

with the implementations:

```cpp
void audio_sample_cb(int16_t left, int16_t right)
{
    if (g_audio != 0)
    {
        int16_t frame[2] = { left, right };
        g_audio->Write(frame, 1);
    }
}

size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    if (g_audio != 0)
    {
        g_audio->Write(data, (unsigned) frames);
    }
    return frames;
}
```

- [ ] **Step 3: Build to verify it compiles and links**

Run: `make`
Expected: rebuilds `src/libretro/callbacks.o`, links cleanly, ends with `COPY  kernel7.img`.

- [ ] **Step 4: Commit**

```bash
git add src/libretro/callbacks.h src/libretro/callbacks.cpp
git commit -m "M6: forward libretro audio callbacks to the AudioDriver

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Kernel integration — audio init + audio-paced loop

**Files:**
- Modify: `src/kernel.h`
- Modify: `src/kernel.cpp`

- [ ] **Step 1: Swap the sound include and member in kernel.h**

In `src/kernel.h`, replace the PWM include (line 21):

```cpp
#include <circle/sound/pwmsounddevice.h>
```

with the AudioDriver include:

```cpp
#include "audio/audio_driver.h"
```

Then replace the member (line 61):

```cpp
	CPWMSoundDevice    m_Sound;      // PWM audio output (M6)
```

with:

```cpp
	AudioDriver        m_Audio;      // HDMI audio output (M6)
```

- [ ] **Step 2: Update the constructor init list in kernel.cpp**

In `src/kernel.cpp`, replace the member init (line 22):

```cpp
	m_Sound (&m_Interrupt),
```

with:

```cpp
	m_Audio (&m_Interrupt),
```

- [ ] **Step 3: Replace the M5 frame loop with the M6 audio-paced loop**

In `src/kernel.cpp`, replace the entire M5 loop block — from the `"M5: entering frame loop"` log line through the end of the `for (;;)` loop (currently lines 166–201):

```cpp
	m_Logger.Write (FromKernel, LogNotice, "M5: entering frame loop");

	// Point the video callback at our Display.
	g_display = &m_Display;

	// Pace to the core's reported frame rate. Approximate for M5; M6 audio
	// will become the real sync source.
	double fps = (double) avInfo.timing.fps;
	if (fps < 1.0) fps = 60.0;
	u64 period_us = (u64) (1000000.0 / fps);

	u64 next = CTimer::GetClockTicks64 ();
	unsigned frame = 0;
	boolean ledOn = FALSE;
	for (;;)
	{
		retro_run ();                       // -> video_refresh_cb -> Blit

		next += period_us;
		u64 now = CTimer::GetClockTicks64 ();
		if (next < now)                     // running behind: drop the slack
		{
			next = now;
		}
		while (CTimer::GetClockTicks64 () < next)
		{
			// spin to the frame deadline
		}

		if (++frame >= 30)                  // ~0.5s liveness blink
		{
			frame = 0;
			ledOn = !ledOn;
			if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
		}
	}
```

with:

```cpp
	m_Logger.Write (FromKernel, LogNotice, "M6: entering frame loop");

	// Pacing parameters from the core's A/V info.
	unsigned sampleRate     = (unsigned) avInfo.timing.sample_rate;
	double   fps            = (double) avInfo.timing.fps;
	if (fps < 1.0) fps = 60.0;
	unsigned framesPerVideo = sampleRate ? (unsigned) (sampleRate / fps) : 0;
	unsigned target         = framesPerVideo * 2;   // ~2 video frames of latency

	// Initialise HDMI audio. On failure, fall back to video-only timer pacing.
	boolean audioOK = (sampleRate > 0) && m_Audio.Initialize (sampleRate);
	if (audioOK)
	{
		g_audio = &m_Audio;
		m_Logger.Write (FromKernel, LogNotice,
			"Audio: HDMI %u Hz, target %u frames", sampleRate, target);
	}
	else
	{
		g_audio = 0;
		m_Logger.Write (FromKernel, LogWarning,
			"Audio disabled; using timer pacing");
	}

	// Point the video callback at our Display.
	g_display = &m_Display;

	u64      period_us = (u64) (1000000.0 / fps);   // timer-fallback period
	u64      next      = CTimer::GetClockTicks64 ();
	unsigned frame     = 0;
	boolean  ledOn     = FALSE;
	for (;;)
	{
		retro_run ();                       // -> video_refresh_cb / audio_*_cb

		if (audioOK)
		{
			// Pace to the audio clock: wait until the queue drains.
			while (m_Audio.QueuedFrames () > target)
			{
				// spin until the hardware has played a frame's worth
			}
		}
		else
		{
			next += period_us;
			u64 now = CTimer::GetClockTicks64 ();
			if (next < now)                 // running behind: drop the slack
			{
				next = now;
			}
			while (CTimer::GetClockTicks64 () < next)
			{
				// spin to the frame deadline
			}
		}

		if (++frame >= 30)                  // ~0.5s liveness blink
		{
			frame = 0;
			ledOn = !ledOn;
			if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
		}
	}
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: rebuilds `src/kernel.o`, links cleanly, ends with `COPY  kernel7.img`. No errors.

- [ ] **Step 5: Confirm the host blit tests still pass (no regression)**

Run: `make -C test run`
Expected: `All blit tests passed`.

- [ ] **Step 6: Commit**

```bash
git add src/kernel.h src/kernel.cpp
git commit -m "M6: HDMI audio init + audio-paced frame loop (timer fallback)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Hardware acceptance

**Files:**
- Create: `docs/m6-hardware-setup.md`

This task has no automated test — it is the on-hardware verification of the milestone.

- [ ] **Step 1: Document the acceptance checklist**

Create `docs/m6-hardware-setup.md`:

```markdown
# M6 Audio Output — Acceptance

## Setup
- Flash the M6 `kernel7.img` to the card (same card as M5; no config.txt HDMI
  changes — native display mode is still used).
- TV connected over HDMI with its speakers/volume on (audio is embedded in the
  HDMI signal).
- Optional: USB-TTL on the serial UART (115200) to see the audio init log line
  ("Audio: HDMI <rate> Hz ..." or "Audio disabled ...").

## Acceptance checklist
- [ ] Sonic demo: music AND sound effects are audible through the TV.
- [ ] No persistent crackle, buzzing, or dropouts during sustained play.
- [ ] Game runs at correct speed and audio/video stay in sync (audio is the
      pacing clock).
- [ ] A few minutes of play show no progressive drift or growing underruns.
- [ ] Fallback: if HDMI audio fails to init, serial shows "Audio disabled" and
      video still runs (timer-paced).

## If audio is absent or glitchy
- Check the serial log for the audio init line (disabled => init failed).
- Crackle/dropouts => tune AudioDriver::QUEUE_MS and the loop `target` (buffer
  depth vs latency).
- Confirm the TV input volume isn't muted and HDMI audio is selected on the TV.
```

- [ ] **Step 2: Commit**

```bash
git add docs/m6-hardware-setup.md
git commit -m "M6: document audio on-hardware acceptance

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

- [ ] **Step 3: Flash and verify on hardware (manual)**

Copy `kernel7.img` to the card, boot the Pi 2 with the TV connected, and work
through the acceptance checklist. If audio is wrong, capture the serial log.

---

## Self-Review Notes

- **Spec coverage:** AudioDriver wrapping CHDMISoundBaseDevice with Write/queue (Task 1) ✓; signed-16 direct feed + both callbacks via g_audio (Task 2) ✓; m_Audio replaces m_Sound, sample-rate init, audio-paced loop, timer fallback, graceful audio-init failure (Task 3) ✓; on-hardware acceptance incl. fallback check (Task 4) ✓; no new unit tests + blit regression guard (Task 3 Step 5) ✓.
- **Type consistency:** `AudioDriver::Initialize/Write/QueuedFrames/IsReady`, `QUEUE_MS`, and `g_audio` names match across Tasks 1–3. `Write(const s16 *, unsigned)` is called with `(int16_t*, unsigned)` — `s16` is Circle's `int16_t`. `audio_batch_cb` casts `size_t frames` to `unsigned` to match.
- **Pacing units:** `target`/`QueuedFrames()` are in frames; `period_us` (fallback) is microseconds with `CTimer::GetClockTicks64()` at 1 MHz — consistent. Fallback spin waits on `< next` (matches M5).
```
