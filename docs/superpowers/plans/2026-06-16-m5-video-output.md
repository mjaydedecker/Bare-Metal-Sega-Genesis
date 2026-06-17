# M5 Video Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the emulated Genesis picture appear on the HDMI display in color at the correct speed by wiring libretro's `video_refresh` callback to a Circle framebuffer.

**Architecture:** A `Display` class owns a fixed 320×240 16bpp `CBcmFrameBuffer`. The Genesis core emits RGB565 frames, which are copied (no conversion) centered into that surface; the Pi firmware GPU-scales the surface to a 4:3-configured HDMI output. The pixel copy lives in a pure, host-testable `blit_rgb565` function. The kernel runs `retro_run()` in a timer-paced loop. Logging moves to the serial UART since the framebuffer is now dedicated to video.

**Tech Stack:** C++, bare-metal Circle (Raspberry Pi 2 / AArch32), Genesis-Plus-GX-Wide libretro core. Host unit tests build with the system C++ compiler.

**Reference spec:** `docs/superpowers/specs/2026-06-16-m5-video-output-design.md`
**Deferred work:** `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `src/video/blit.h` / `blit.cpp` (new) | Pure `blit_rgb565` — centered RGB565 row copy. No Circle types. Host-testable. |
| `src/video/display.h` / `display.cpp` (new) | `Display` class — owns the `CBcmFrameBuffer`, clears bars on geometry change, delegates the copy to `blit_rgb565`. |
| `src/libretro/callbacks.{h,cpp}` (modify) | Implement `video_refresh_cb`; add `g_display` pointer. |
| `src/kernel.{h,cpp}` (modify) | Add `m_Display`, init it, route logging to serial, replace the M4 heartbeat with the frame loop. |
| `Makefile` (modify) | Add the new objects to `OBJS` and to `EXTRACLEAN`. |
| `test/test_blit.cpp`, `test/Makefile` (new) | Host unit test for `blit_rgb565`. |

---

## Task 1: Pure `blit_rgb565` function (host-tested, TDD)

**Files:**
- Create: `src/video/blit.h`
- Create: `src/video/blit.cpp`
- Create: `test/test_blit.cpp`
- Create: `test/Makefile`

- [ ] **Step 1: Write the header (declaration only)**

Create `src/video/blit.h`:

```cpp
//
// src/video/blit.h
//
// Bare Metal Sega Genesis
// Pure RGB565 centered blit — no Circle dependencies, host-testable.
//

#ifndef _video_blit_h
#define _video_blit_h

#include <stdint.h>
#include <stddef.h>

// Copy a w*h RGB565 image, centered, into a dst_w*dst_h RGB565 surface.
// Strides are in BYTES. src may have a stride wider than w (the core's
// buffer pitch is fixed at 720*2 while the active width varies).
// src == NULL is a no-op (libretro "repeat last frame"). w/h are clamped
// to the destination. Caller is responsible for clearing letterbox bars.
void blit_rgb565(uint16_t *dst, unsigned dst_pitch_bytes,
                 unsigned dst_w, unsigned dst_h,
                 const uint16_t *src, unsigned src_pitch_bytes,
                 unsigned w, unsigned h);

#endif
```

- [ ] **Step 2: Write the failing test**

Create `test/test_blit.cpp`:

```cpp
#include "../src/video/blit.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define DST_W 320u
#define DST_H 240u
#define SRC_STRIDE_PX 720u   // core's fixed buffer width

static uint16_t dst[DST_W * DST_H];
static uint16_t src[SRC_STRIDE_PX * DST_H];

static void reset_buffers(void) {
    memset(dst, 0, sizeof(dst));
    memset(src, 0, sizeof(src));
}

// Centering: 256x224 into 320x240 -> offX=32, offY=8.
static void test_centering_256x224(void) {
    reset_buffers();
    // Mark each source row's first pixel with a unique value.
    for (unsigned y = 0; y < 224; y++)
        src[y * SRC_STRIDE_PX + 0] = (uint16_t)(y + 1);
    src[223 * SRC_STRIDE_PX + 255] = 0xBEEF; // last visible pixel

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 256, 224);

    unsigned offX = (DST_W - 256) / 2; // 32
    unsigned offY = (DST_H - 224) / 2; // 8
    assert(dst[(offY + 0) * DST_W + offX] == 1);
    assert(dst[(offY + 10) * DST_W + offX] == 11);
    assert(dst[(offY + 223) * DST_W + (offX + 255)] == 0xBEEF);
    // Outside the centered region stays untouched (caller clears bars).
    assert(dst[0] == 0);
    printf("test_centering_256x224 OK\n");
}

// Source stride must be honored (720, not width=256).
static void test_source_stride_honored(void) {
    reset_buffers();
    for (unsigned y = 0; y < 224; y++)
        src[y * SRC_STRIDE_PX] = (uint16_t)(0x100 + y);

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 256, 224);

    unsigned offX = 32, offY = 8;
    // Row 1's marker would be wrong if stride were treated as 256.
    assert(dst[(offY + 1) * DST_W + offX] == 0x101);
    assert(dst[(offY + 5) * DST_W + offX] == 0x105);
    printf("test_source_stride_honored OK\n");
}

// Full-width 320x240 -> no offset.
static void test_full_320x240(void) {
    reset_buffers();
    src[0] = 0xAAAA;
    src[239 * SRC_STRIDE_PX + 319] = 0x5555;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 320, 240);

    assert(dst[0] == 0xAAAA);
    assert(dst[239 * DST_W + 319] == 0x5555);
    printf("test_full_320x240 OK\n");
}

// Oversized input is clamped (no overflow, no crash).
static void test_clamp_oversized(void) {
    reset_buffers();
    for (unsigned i = 0; i < SRC_STRIDE_PX * DST_H; i++) src[i] = 0x1234;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 400, 300); // exceeds surface

    assert(dst[0] == 0x1234);
    assert(dst[DST_W * DST_H - 1] == 0x1234);
    printf("test_clamp_oversized OK\n");
}

// NULL source is a no-op (dupe frame).
static void test_null_src_noop(void) {
    reset_buffers();
    for (unsigned i = 0; i < DST_W * DST_H; i++) dst[i] = 0x4321;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                NULL, SRC_STRIDE_PX * 2, 256, 224);

    assert(dst[0] == 0x4321);
    assert(dst[DST_W * DST_H - 1] == 0x4321);
    printf("test_null_src_noop OK\n");
}

int main(void) {
    test_centering_256x224();
    test_source_stride_honored();
    test_full_320x240();
    test_clamp_oversized();
    test_null_src_noop();
    printf("All blit tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Write the test Makefile**

Create `test/Makefile`:

```makefile
# Host-side unit tests (system compiler, not the cross toolchain).
CXX      ?= c++
CXXFLAGS ?= -Wall -Wextra -O2

.PHONY: run clean

run: test_blit
	./test_blit

test_blit: test_blit.cpp ../src/video/blit.cpp ../src/video/blit.h
	$(CXX) $(CXXFLAGS) -o $@ test_blit.cpp ../src/video/blit.cpp

clean:
	rm -f test_blit
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test run`
Expected: link error — `undefined reference to 'blit_rgb565'` (no implementation yet).

- [ ] **Step 5: Implement `blit_rgb565`**

Create `src/video/blit.cpp`:

```cpp
//
// src/video/blit.cpp
//
// Bare Metal Sega Genesis
// Pure RGB565 centered blit. See blit.h.
//

#include "blit.h"
#include <string.h>

void blit_rgb565(uint16_t *dst, unsigned dst_pitch_bytes,
                 unsigned dst_w, unsigned dst_h,
                 const uint16_t *src, unsigned src_pitch_bytes,
                 unsigned w, unsigned h)
{
    if (dst == 0 || src == 0) return;          // dupe frame / no surface
    if (w > dst_w) w = dst_w;                   // clamp defensively
    if (h > dst_h) h = dst_h;

    unsigned off_x      = (dst_w - w) / 2;
    unsigned off_y      = (dst_h - h) / 2;
    unsigned dst_stride = dst_pitch_bytes / 2;  // in pixels
    unsigned src_stride = src_pitch_bytes / 2;

    for (unsigned y = 0; y < h; y++)
    {
        memcpy(dst + (off_y + y) * dst_stride + off_x,
               src + y * src_stride,
               (size_t) w * 2);
    }
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test run`
Expected: prints each `... OK` line and `All blit tests passed`, exit code 0.

- [ ] **Step 7: Commit**

```bash
git add src/video/blit.h src/video/blit.cpp test/test_blit.cpp test/Makefile
git commit -m "M5: add host-tested RGB565 centered blit

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: `Display` class wrapping `CBcmFrameBuffer`

**Files:**
- Create: `src/video/display.h`
- Create: `src/video/display.cpp`
- Modify: `Makefile` (`OBJS`, `EXTRACLEAN`)

- [ ] **Step 1: Write the Display header**

Create `src/video/display.h`:

```cpp
//
// src/video/display.h
//
// Bare Metal Sega Genesis
// Owns a fixed 320x240 16bpp framebuffer and blits RGB565 frames into it,
// centered. The Pi firmware GPU-scales the surface to the HDMI output.
//

#ifndef _video_display_h
#define _video_display_h

#include <circle/bcmframebuffer.h>
#include <circle/types.h>

class Display
{
public:
    static const unsigned FB_WIDTH  = 320;
    static const unsigned FB_HEIGHT = 240;   // 320x240 is exactly 4:3
    static const unsigned FB_DEPTH  = 16;    // RGB565

    Display(void);
    ~Display(void);

    boolean Initialize(void);

    // Copy one RGB565 frame, centered. pitch is the source row stride in bytes.
    void Blit(const void *src, unsigned width, unsigned height, size_t pitch);

private:
    void ClearBlack(void);

    CBcmFrameBuffer *m_pFB;
    u16             *m_pBuffer;   // framebuffer base
    unsigned         m_Pitch;     // framebuffer pitch in bytes
    unsigned         m_LastW;
    unsigned         m_LastH;
};

#endif
```

- [ ] **Step 2: Write the Display implementation**

Create `src/video/display.cpp`:

```cpp
//
// src/video/display.cpp
//
// Bare Metal Sega Genesis
// See display.h.
//

#include "display.h"
#include "blit.h"
#include <circle/util.h>
#include <stdint.h>

Display::Display(void)
:   m_pFB(0), m_pBuffer(0), m_Pitch(0), m_LastW(0), m_LastH(0)
{
}

Display::~Display(void)
{
    delete m_pFB;
    m_pFB = 0;
}

boolean Display::Initialize(void)
{
    m_pFB = new CBcmFrameBuffer(FB_WIDTH, FB_HEIGHT, FB_DEPTH);
    if (m_pFB == 0)
    {
        return FALSE;
    }
    if (!m_pFB->Initialize() || m_pFB->GetBuffer() == 0)
    {
        delete m_pFB;
        m_pFB = 0;
        return FALSE;
    }

    m_pBuffer = (u16 *) (uintptr_t) m_pFB->GetBuffer();
    m_Pitch   = m_pFB->GetPitch();
    m_LastW   = 0;
    m_LastH   = 0;
    ClearBlack();
    return TRUE;
}

void Display::ClearBlack(void)
{
    if (m_pBuffer != 0)
    {
        memset(m_pBuffer, 0, (size_t) m_Pitch * FB_HEIGHT);
    }
}

void Display::Blit(const void *src, unsigned width, unsigned height, size_t pitch)
{
    if (m_pBuffer == 0 || src == 0)   // no surface, or dupe frame
    {
        return;
    }

    if (width != m_LastW || height != m_LastH)
    {
        ClearBlack();                 // repaint letterbox/pillarbox bars
        m_LastW = width;
        m_LastH = height;
    }

    blit_rgb565(m_pBuffer, m_Pitch, FB_WIDTH, FB_HEIGHT,
                (const uint16_t *) src, (unsigned) pitch, width, height);
}
```

- [ ] **Step 3: Add the new objects to the Makefile**

In `Makefile`, modify the `OBJS` list (currently ends at `src/stdlib_stubs.o`) to add the two video objects:

```makefile
OBJS = src/main.o \
       src/kernel.o \
       src/storage/sdcard.o \
       src/libretro/environment.o \
       src/libretro/callbacks.o \
       src/runtime_stubs.o \
       src/cstdlib_stubs.o \
       src/stdlib_stubs.o \
       src/video/blit.o \
       src/video/display.o
```

And extend `EXTRACLEAN` to include the video objects:

```makefile
EXTRACLEAN = src/*.o src/*.d src/storage/*.o src/storage/*.d \
             src/libretro/*.o src/libretro/*.d \
             src/video/*.o src/video/*.d \
             build/genesis libs/libgenesis.a
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: compiles `src/video/blit.o` and `src/video/display.o`, links, ends with `COPY  kernel7.img`. No errors.

- [ ] **Step 5: Commit**

```bash
git add src/video/display.h src/video/display.cpp Makefile
git commit -m "M5: add Display class owning a 320x240 16bpp framebuffer

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Wire `video_refresh_cb` to the Display

**Files:**
- Modify: `src/libretro/callbacks.h`
- Modify: `src/libretro/callbacks.cpp`

- [ ] **Step 1: Declare `g_display` in the callbacks header**

In `src/libretro/callbacks.h`, after the `#include <libretro.h>` line (line 12), add a forward declaration and the global:

```cpp
#include <libretro.h>

// Set by the kernel before the frame loop; video_refresh_cb forwards
// frames here. Forward-declared to avoid pulling Circle into this header.
class Display;
extern Display *g_display;
```

- [ ] **Step 2: Define `g_display` and implement the callback**

In `src/libretro/callbacks.cpp`, add the include and definition near the top (after the existing `#include "callbacks.h"`):

```cpp
#include "callbacks.h"
#include "../video/display.h"

Display *g_display = 0;
```

Then replace the existing no-op `video_refresh_cb` body:

```cpp
void video_refresh_cb(const void *data, unsigned width, unsigned height,
                      size_t pitch)
{
    if (g_display != 0)
    {
        g_display->Blit(data, width, height, pitch);
    }
}
```

(Leave `audio_sample_cb`, `audio_batch_cb`, `input_poll_cb`, `input_state_cb` as their current no-op stubs — those are M6/M7.)

- [ ] **Step 3: Build to verify it compiles and links**

Run: `make`
Expected: rebuilds `src/libretro/callbacks.o`, links cleanly, ends with `COPY  kernel7.img`.

- [ ] **Step 4: Commit**

```bash
git add src/libretro/callbacks.h src/libretro/callbacks.cpp
git commit -m "M5: forward video_refresh_cb frames to the Display

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Kernel integration — Display, serial logging, frame loop

**Files:**
- Modify: `src/kernel.h`
- Modify: `src/kernel.cpp`

- [ ] **Step 1: Add the Display member and include**

In `src/kernel.h`, add the include alongside the other project includes (after `#include "libretro/callbacks.h"`):

```cpp
#include "video/display.h"
```

Then add the member to `CKernel`'s private section, immediately after `CPWMSoundDevice m_Sound;` (the Display has a trivial constructor, so ordering is safe):

```cpp
	CPWMSoundDevice    m_Sound;      // PWM audio output (M6)
	Display            m_Display;    // HDMI video output (M5)
```

- [ ] **Step 2: Route logging to the serial UART**

In `src/kernel.cpp`, in `CKernel::Initialize()`, change the logger fallback target from the screen to the serial device. Find this block (around line 52):

```cpp
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}
```

Replace `&m_Screen` with `&m_Serial`:

```cpp
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Serial;   // M5: framebuffer is dedicated to video
		}
```

- [ ] **Step 3: Initialize the Display in Initialize()**

In `src/kernel.cpp`, `CKernel::Initialize()` ends with the SD-card block followed by `return bOK;`:

```cpp
	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising SD card");
		bOK = m_EMMC.Initialize ();
	}

	return bOK;
}
```

Insert the Display init block between the SD-card block and `return bOK;`:

```cpp
	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising SD card");
		bOK = m_EMMC.Initialize ();
	}

	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising video");
		bOK = m_Display.Initialize ();
		if (!bOK)
		{
			m_Logger.Write (FromKernel, LogPanic, "Display init failed");
		}
	}

	return bOK;
}
```

- [ ] **Step 4: Replace the heartbeat with the frame loop**

In `src/kernel.cpp`, in `CKernel::Run()`, replace the M4 heartbeat block (currently lines 156–164):

```cpp
	m_Logger.Write (FromKernel, LogNotice, "M4 complete — core loaded.");

	// Heartbeat until M5 (video) is implemented.
	for (unsigned i = 0; ; i++)
	{
		m_Timer.SimpleMsDelay (1000);
		if (i % 2 == 0) m_ActLED.On ();
		else             m_ActLED.Off ();
	}

	return ShutdownHalt;
```

with the M5 frame loop:

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

	return ShutdownHalt;
```

(`avInfo` is the local already populated by `retro_get_system_av_info()` earlier in `Run()`. `CTimer` is already included via `kernel.h`.)

- [ ] **Step 5: Build to verify it compiles and links**

Run: `make`
Expected: rebuilds `src/kernel.o`, links cleanly, ends with `COPY  kernel7.img`. No errors.

- [ ] **Step 6: Confirm the host blit tests still pass (no regression)**

Run: `make -C test run`
Expected: `All blit tests passed`.

- [ ] **Step 7: Commit**

```bash
git add src/kernel.h src/kernel.cpp
git commit -m "M5: initialise Display, route logging to serial, run frame loop

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Card configuration & on-hardware acceptance

**Files:**
- Create: `docs/m5-hardware-setup.md`

This task has no automated test — it is the on-hardware verification of the milestone. Record the card setup so it is reproducible.

- [ ] **Step 1: Document the card config and acceptance steps**

Create `docs/m5-hardware-setup.md`:

```markdown
# M5 Hardware Setup & Acceptance

## Card files (FAT root)
- `kernel7.img` (the M5 build)
- Firmware: `bootcode.bin`, `start.elf`, `fixup.dat`
- `GAME.MD` — a test Genesis ROM
- `config.txt` — see below

## config.txt — force a 4:3 HDMI mode
The kernel renders a fixed 320x240 (4:3) surface; the firmware scales it to the
HDMI signal. Force a 4:3 signal so the TV pillarboxes it (tune to your panel):

    hdmi_group=2
    hdmi_mode=16     # 1024x768 (4:3). Alt: hdmi_mode=35 (1280x960).

Set one mode, verify on the TV, adjust if the panel rejects it. No code depends
on the exact mode.

## Serial logging
Logging now goes to the serial UART at 115200 (GPIO14/15). Attach a USB-TTL
adapter to see boot diagnostics; the HDMI output is dedicated to video.

## Acceptance checklist
- [ ] A known ROM boots to its title/first frame, visible and centered.
- [ ] Runs at correct speed (time a known-duration intro).
- [ ] An H32 (256-wide) game and an H40 (320-wide) game both display correctly.
- [ ] A game that switches resolution mid-run does not corrupt or crash.
- [ ] Serial log shows the expected boot sequence with no panics.
```

- [ ] **Step 2: Commit**

```bash
git add docs/m5-hardware-setup.md
git commit -m "M5: document card config and on-hardware acceptance

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

- [ ] **Step 3: Flash and verify on hardware (manual)**

Copy `kernel7.img` to the card root, add the `config.txt` 4:3 mode and `GAME.MD`, boot the Pi 2, and work through the acceptance checklist above. If video is absent or wrong, capture the serial log for debugging.

---

## Self-Review Notes

- **Spec coverage:** Display class + fixed 320×240 16bpp FB (Task 2) ✓; direct RGB565 blit, centering, clear-on-geometry-change, dupe-frame/clamp guards (Tasks 1–2) ✓; `g_display` + `video_refresh_cb` (Task 3) ✓; serial logging, frame loop, timer pacing, halt-on-init-failure (Task 4) ✓; config.txt 4:3 + acceptance (Task 5) ✓; host tests for the blit math (Task 1) ✓.
- **Type consistency:** `blit_rgb565` signature identical in `blit.h`, the test, and `display.cpp`'s call. `Display::Blit`, `Display::Initialize`, `FB_WIDTH/HEIGHT/DEPTH`, and `g_display` names match across Tasks 2–4.
- **Pacing units:** `period_us` in microseconds; `CTimer::GetClockTicks64()` runs at `CLOCKHZ = 1_000_000` (1 µs/tick) — consistent.
```
