# M5 — Video Output: Design

**Date:** 2026-06-16
**Milestone:** M5 (Video Output)
**Status:** design approved; implementation plan to follow.
**Target hardware:** Raspberry Pi 2 (`RASPPI=2`, AArch32, `kernel7.img`).

## Goal

Make the emulated Genesis picture visible on the HDMI display, in color and at
the correct speed, by wiring the libretro `video_refresh` callback to Circle's
framebuffer. This proves end-to-end video output; richer features are
deliberately deferred (see Deferred Work).

**Done when:** a known ROM boots to its first frame, centered and running at the
right speed; H32 (256-wide) and H40 (320-wide) games both display correctly,
including a game that switches modes mid-run; the serial log shows a clean boot
with no panics.

## Decisions (and the alternatives we rejected)

| Decision | Choice | Rejected alternative (deferred) |
|----------|--------|---------------------------------|
| Where scaling happens | **GPU upscale** — firmware scales a small framebuffer to the panel | CPU integer scaling at panel resolution (heavy on Pi 2) — backlog #1 |
| Framebuffer depth | **16bpp**, blit core RGB565 directly, no conversion | 32bpp ARGB8888 with per-pixel conversion |
| Resolution handling | **Fixed 320×240 surface (Approach A)**; re-center the active frame inside it | Resize framebuffer to native on each mode change (Approach B) — backlog #2 |
| Aspect ratio | **Preserve 4:3** (pillarboxed by the TV) | Stretch-to-fill; configurable integer/stretch — backlog #4 |
| Scope | **Minimal**: frames on screen, full speed | FSD video options (widescreen, toggles) now — backlog #4, #5 |

The Genesis core already emits **RGB565** (`FRONTEND_SUPPORTS_RGB565=1` +
`USE_16BPP_RENDERING`; `libretro.c:3520` issues
`RETRO_ENVIRONMENT_SET_PIXEL_FORMAT` with `RETRO_PIXEL_FORMAT_RGB565`). A 16bpp
framebuffer therefore lets us copy frames with no per-pixel conversion.

## Architecture

```
CKernel ──owns──> Display ──owns──> CBcmFrameBuffer (320×240, 16bpp)
   │
   └──sets──> g_display ──used by──> video_refresh_cb ──calls──> Display::Blit()
```

- **`Display`** (`src/video/display.{h,cpp}`) — owns video output; knows nothing
  about libretro. Pure framebuffer sink, so its blit logic is testable in
  isolation.
- **`video_refresh_cb`** (`src/libretro/callbacks.cpp`) — forwards frames to the
  `Display` via a `g_display` global, matching the existing global-pointer
  pattern (`g_rom_data`, etc.).
- **`CKernel`** — adds a `Display m_Display` member, initializes it, sets
  `g_display`, and runs the frame loop.

Dependency direction: `kernel → display`, `kernel → callbacks → (g_display) →
display`. `Display` depends only on Circle's `CBcmFrameBuffer`.

## Components

### `Display` (`src/video/display.h` / `.cpp`)

Constants: `FB_WIDTH = 320`, `FB_HEIGHT = 240` (exactly 4:3).

```cpp
class Display {
public:
    bool Initialize();   // create framebuffer, verify, clear to black
    void Blit(const void *src, unsigned width, unsigned height, size_t pitch);
private:
    CBcmFrameBuffer *m_pFB     = nullptr;
    u16             *m_pBuffer = nullptr;   // GetBuffer()
    unsigned         m_Pitch   = 0;         // GetPitch() in bytes
    unsigned         m_LastW   = 0;
    unsigned         m_LastH   = 0;
};
```

- `Initialize()`: construct `CBcmFrameBuffer(320, 240, 16)`; if `GetBuffer()==0`
  return false; cache base pointer and pitch; clear to black.
- `Blit()`: thin wrapper that computes the framebuffer destination and delegates
  to the pure `blit_rgb565` function (below).

### Pure blit function (host-testable)

```cpp
// No Circle types — just pointers and dimensions.
void blit_rgb565(u16 *dst, unsigned dst_pitch_bytes,
                 unsigned dst_w, unsigned dst_h,
                 const u16 *src, unsigned src_pitch_bytes,
                 unsigned w, unsigned h);
```

Behavior:
- `src == nullptr` → no-op (libretro dupe-frame: "repeat last frame").
- Clamp `w` to `dst_w`, `h` to `dst_h` (defensive; not expected with widescreen
  off).
- Centering offsets: `offX = (dst_w - w) / 2`, `offY = (dst_h - h) / 2`.
- Row copy: for each `y` in `0..h`,
  `memcpy(dst + (offY+y)*dst_pitch_bytes/2 + offX, src + y*src_pitch_bytes/2, w*2)`.
- **Source stride is `src_pitch_bytes`, not `w*2`** — the core's buffer pitch is
  fixed at `720*2` while the active width varies.
- Clearing the bars is the caller's responsibility (see below), not part of the
  copy.

### `video_refresh_cb` (`src/libretro/callbacks.cpp`)

```cpp
Display *g_display = nullptr;
void video_refresh_cb(const void *data, unsigned width, unsigned height,
                      size_t pitch) {
    if (g_display) g_display->Blit(data, width, height, pitch);
}
```

### `CKernel` changes (`src/kernel.{h,cpp}`)

- Add `Display m_Display;` member.
- In `Initialize()`: call `m_Display.Initialize()`; on failure log `LogPanic`
  (serial) and return false.
- **Logging moves to the serial UART** (115200) once video owns the screen, so
  diagnostics stay visible over USB-TTL. Boot logs before `Display::Initialize()`
  may still use the screen console.
- In `Run()`: set `g_display = &m_Display`, then replace the M4 heartbeat loop
  with the frame loop.

## Data flow — per frame

1. `retro_run()` advances emulation one frame and calls `video_refresh_cb`.
2. `video_refresh_cb` → `Display::Blit(src, w, h, pitch)`.
3. `Blit`:
   - If geometry changed since last frame (or first frame), clear the whole
     320×240 framebuffer to black — this paints the pillarbox/letterbox bars.
     Steady-state frames skip the clear.
   - Update `m_LastW/m_LastH`; call `blit_rgb565` to copy the centered frame.
4. The firmware continuously scales the 320×240 surface to the 4:3 HDMI output.

## Frame loop & pacing

```
g_display = &m_Display;
const u64 period_us = 1000000 / fps;        // fps from av_info (~16694 NTSC)
u64 next = m_Timer.GetClockTicks();          // 1 MHz ticks
for (;;) {
    retro_run();                             // → video_refresh_cb → Blit
    next += period_us;
    if (next < m_Timer.GetClockTicks())      // running behind: don't accrue debt
        next = m_Timer.GetClockTicks();
    while (m_Timer.GetClockTicks() < next) { /* spin/short delay */ }
    // periodic ACT-LED toggle as liveness
}
```

- Pacing is timer-based to the core's reported `fps`. It is **approximate for M5**
  and a placeholder: **M6 audio will become the real sync source**.
- If a frame runs long, slack is dropped (clamp `next` to now) rather than
  accumulated, so the game does not spiral.
- Input callbacks remain no-op stubs (M7).

## HDMI / `config.txt` setup (4:3)

The 320×240 framebuffer is scaled by the firmware to the active HDMI mode. To
keep the picture 4:3, force a 4:3 HDMI mode on the SD card's `config.txt` so the
signal itself is 4:3 and the TV pillarboxes it:

- `hdmi_group=2` plus a 4:3 DMT `hdmi_mode`. Suggested starting points:
  `hdmi_mode=16` (1024×768) or `hdmi_mode=35` (1280×960).
- **This is a hardware-tuning step:** set one 4:3 mode, verify on the TV, adjust
  if the panel rejects it. No code depends on the exact mode — `Display` always
  renders 320×240 and the firmware scales. Robust multi-panel handling is
  deferred (backlog #7).

## Error handling

- `Display::Initialize()` failure (`GetBuffer()==0`) → `LogPanic` to serial and
  halt with a clear message, mirroring the SD-mount / ROM-load failure pattern in
  `kernel.cpp`.
- `Blit`: `src==NULL` → skip (dupe frame); out-of-range `w/h` → clamp. No
  allocation in the hot path, so no per-frame failure modes.

## Testing

### Host unit test (automated, off-target)

Test `blit_rgb565` with the system gcc on the dev machine (TDD: tests first):
- centering offsets for 320×224, 256×224, 320×240, 256×240;
- source `pitch` ≠ `width*2` is honored (the `720*2` stride case);
- clamping when `w/h` exceed the surface;
- `src==NULL` is a no-op.

The project has no test harness yet, so this adds a minimal standalone `test/`
make target.

### On-hardware acceptance

- A known ROM boots to its title/first frame, visible and centered.
- Runs at correct speed (timed against a known-duration intro).
- An H32 (256-wide) and an H40 (320-wide) game both display correctly; a
  mode-switching game does not corrupt or crash.
- Serial log shows the expected boot sequence and no panics.

## Files

- `src/video/display.h` — new (`Display` class + `blit_rgb565` declaration).
- `src/video/display.cpp` — new.
- `src/libretro/callbacks.cpp` / `.h` — implement `video_refresh_cb`, add
  `g_display`.
- `src/kernel.h` / `kernel.cpp` — `m_Display` member, init, serial logging, frame
  loop.
- `Makefile` — add `src/video/display.o` to `OBJS`; add the host `test/` target.
- `test/` — new minimal host test for `blit_rgb565`.
- Card `config.txt` — 4:3 HDMI mode (hardware-tuning step, not in repo build).

## Deferred work

Higher-order alternatives weighed during brainstorming are recorded in
[`2026-06-16-video-output-deferred-enhancements.md`](2026-06-16-video-output-deferred-enhancements.md):
CPU integer-scaling path (#1), authentic-fill via native-resolution framebuffer
(#2), PAR correctness (#3), integer/stretch toggle (#4), widescreen (#5),
vsync/double-buffering (#6), config-driven video settings (#7), and on-screen
logging overlay (#8).
