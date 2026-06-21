# Boot Splash Screen — Design

**Date:** 2026-06-21
**Status:** approved design, pending implementation plan
**Related:** FSD §(splash screen — "Branded boot screen during Circle hardware
initialization"); the M5 video pipeline ([[project_m5_video_status]]) and
`src/video/blit.h` (centered integer blit it reuses); `src/video/display.{h,cpp}`,
`src/ui/text_canvas.{h,cpp}`; the boot sequence in `src/kernel.cpp`.

## Summary

Show a branded **logo bitmap** on screen as early as possible during boot, so it
masks the slow USB-enumeration + SD-mount wait, then hands off to the ROM browser
the instant boot finishes. The logo is a raw **RGB565** image: an **embedded**
default compiled into the kernel, optionally **overridden** by `SD:/splash.raw` if
present. Logo only — black background, centered, integer-scaled — **no text**. No
minimum display time (a fast boot may only flash the logo briefly, which is
accepted).

## Decisions (from brainstorming)

- **Content:** a single logo bitmap, centered on black. No title/version text.
- **Format:** raw RGB565, no decode (decoding PNG/JPEG on bare metal is out of
  scope). A tiny header carries the dimensions so any size works.
- **Source:** embedded default **with SD override** (`SD:/splash.raw`).
- **Timing:** cover hardware init (Display brought up before USB/SD), **no minimum
  display time** — the browser's first draw dismisses it.
- **Artwork:** a generated placeholder for now; real art swapped in later via the
  converter tool or the SD override.

## Asset Format (`SGSP`)

Both the embedded blob and the SD file use one layout:

| Offset | Size | Field                                   |
|--------|------|-----------------------------------------|
| 0      | 4    | magic `'S','G','S','P'`                  |
| 4      | 2    | `width`  (u16, little-endian)            |
| 6      | 2    | `height` (u16, little-endian)            |
| 8      | w*h*2| RGB565 pixels, row-major, little-endian  |

Total length is exactly `8 + width*height*2`.

The **embedded** asset is a generated, committed C file (no build-time tool
dependency) exposing the same pixels as a flat array:

```c
// src/video/splash_data.h
extern const unsigned       g_splash_w;
extern const unsigned       g_splash_h;
extern const unsigned short g_splash_data[];   // g_splash_w * g_splash_h RGB565
```

(The embedded form skips the 8-byte header — the dimensions are separate symbols;
the header exists only for the self-describing SD file.)

Keep the embedded image **modest** (e.g. ~160×100 ≈ 32 KB) to stay clear of the
`KERNEL_MAX_SIZE` boot-crash ceiling ([[project_m4_build_fixes]]).

## Converter Tool (`tools/mksplash.py`)

A host dev tool, **not** part of the kernel build:

- `mksplash.py <in.png> --c src/video/splash_data.c` — convert a PNG to the
  committed embedded C file (`g_splash_w/h/data`). Requires Pillow on the dev box.
- `mksplash.py <in.png> --raw splash.raw` — emit an `SGSP` file for the SD card.
- `mksplash.py --placeholder --c src/video/splash_data.c` — generate the starter
  placeholder **without any input image** (pure procedural: a dark field with
  three colored vertical bars — a simple Genesis-flavored mark, no text), so the
  pipeline ships and is testable immediately.

Generated output is committed to the repo; the tool is only re-run to change art.

## Module: `src/video/splash.{h,cpp}`

Pure, host-testable helpers (no Circle dependency) plus thin Circle-side draws.

```c
// Parsed view over an SGSP buffer (pixels point INTO the caller's buffer).
struct SplashImage { unsigned w; unsigned h; const unsigned short *pixels; };

// Validate an SGSP buffer: magic, and length == 8 + w*h*2. On success fills out
// and returns true; on any mismatch returns false and leaves out untouched.
bool splash_parse(const unsigned char *buf, size_t len, SplashImage *out);

// Largest integer scale so w*scale <= fbW*7/10 AND h*scale <= fbH*7/10 (min 1).
unsigned splash_scale(unsigned fbW, unsigned fbH, unsigned imgW, unsigned imgH);
```

Circle-side draw (declared in splash.h, defined in splash.cpp):

```c
// Clear the framebuffer to black and blit the EMBEDDED logo centered,
// integer-scaled by splash_scale(). Safe no-op if the display has no surface.
void splash_show_embedded(TextCanvas *canvas, Display *display);

// If SD:/splash.raw exists and parses as a valid SGSP image that fits, clear +
// blit it (replacing the embedded logo). Otherwise leave the screen unchanged.
void splash_apply_override(Storage *storage, TextCanvas *canvas, Display *display);
```

**Draw mechanics (shared):** `canvas->Clear(0x0000)` to black, then
`blit_rgb565(display->Buffer(), display->Pitch(), display->Width(),
display->Height(), pixels, w*2, w, h, splash_scale(...))`. `blit_rgb565` already
**centers** the image and reduces `scale` if it somehow wouldn't fit, so no
placement math is needed beyond `splash_scale`.

`splash_apply_override` uses `Storage::Exists` + `Storage::ReadFile(path, &buf,
&size)` (the existing allocating read used for ROMs), runs `splash_parse`, draws
on success, and frees the buffer. A missing file, bad magic, wrong size, or a
read failure all leave the embedded logo on screen.

## Boot Integration (`src/kernel.cpp`)

Today `CKernel::Initialize` runs: Screen → Serial → Logger → Interrupt → Timer →
**USBHCI → EMMC → Display**. USB enumeration and SD mount (the slow part) finish
*before* the framebuffer exists, so a `Display`-drawn splash can't cover them.

**Reorder:** move the `Display.Initialize()` block to **right after
`Timer.Initialize()`**, before the USBHCI block. `Display` only needs the VideoCore
mailbox (no USB/EMMC dependency), so this is safe. Immediately after a successful
`Display.Initialize()`, call `splash_show_embedded(&m_Canvas, &m_Display)`.

Console logging (`m_Logger.Write`) during the subsequent USB/SD init is harmless to
the splash: the gameplay loop already issues `Logger.Write` calls while the
`Display` framebuffer is live without corrupting it ([[project_serial_logging_gotcha]]),
so the same holds during boot.

**SD override:** in `CKernel::Run`, immediately after `m_Storage.Mount()` succeeds
(before settings load), call `splash_apply_override(&m_Storage, &m_Canvas,
&m_Display)`. This redraws with `SD:/splash.raw` if present, for the remainder of
boot (settings load + `retro_init`).

**Dismissal:** none needed. The first ROM-browser render (`m_RomMenu.Run`, or the
auto-launch game's first frame) clears and draws over the splash. No timer, no
minimum.

## Error Handling

- `Display.Initialize()` fails → existing `LogPanic` path; no splash (nothing to
  draw on). Boot behavior unchanged from today.
- Embedded asset is always present (compiled in), so the early splash always shows
  once `Display` is up.
- SD override absent/unreadable/malformed (bad magic, wrong length, or an image
  too large to fit at scale 1) → `splash_apply_override` is a no-op; the embedded
  logo remains. No error surfaced to the user.
- SD mount itself fails → existing `LogPanic`/halt; the embedded splash stays on
  screen behind the panic log.

## Testing

**Host unit tests** (`test/test_splash.cpp`, added to `test/Makefile`):
- `splash_parse`: a hand-built valid `SGSP` buffer parses to the right `w/h` and
  pixel pointer; bad magic rejected; `len` shorter/longer than `8 + w*h*2`
  rejected; a zero-length / too-short buffer rejected.
- `splash_scale`: exact cases — e.g. fb 1280×720, img 160×100 → scale 5
  (`160*5=800 ≤ 896`, `100*5=500 ≤ 504`; 6 would exceed); img larger than 70% at
  scale 1 → returns 1; a tiny img on a large fb → the 70%-bounded scale; never
  returns 0.

(The draws and the boot reorder are Circle/hardware — verified on hardware, like
the rest of `display.cpp` / `kernel.cpp`. Build verification is the repo-root
`make` producing `kernel7.img`.)

**Hardware verification** (new checklist item):
- Power on → the logo appears early and stays through the USB/SD wait, then the
  ROM browser replaces it.
- Place a valid `SD:/splash.raw` (made with `mksplash.py --raw`) → after SD mounts,
  that image replaces the embedded logo.
- Remove/scramble `SD:/splash.raw` → the embedded logo shows for the whole boot;
  no hang or corruption.

## Out of Scope

- PNG/JPEG decoding on the device (raw RGB565 only; conversion is offline).
- Title/version/credits text, animation, progress bar, or fade.
- A minimum display duration or button-to-skip.
- Per-image positioning options (always centered) or non-integer scaling.

## Cross-references

- Centered integer blit reused — `src/video/blit.h` (`blit_rgb565`).
- M5 video / framebuffer = output mode — [[project_m5_video_status]].
- Kernel-size ceiling for the embedded asset — [[project_m4_build_fixes]].
- FSD splash-screen requirement — `Documents/Bare-Metal-Sega-Genesis-FSD.md`.
</content>
