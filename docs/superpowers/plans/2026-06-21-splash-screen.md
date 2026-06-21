# Boot Splash Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show an embedded RGB565 logo (overridable by `SD:/splash.raw`) on screen early in boot to mask the USB/SD init wait, then yield to the ROM browser.

**Architecture:** Pure host-tested helpers (`splash_parse`, `splash_scale`) in `splash.cpp`; Circle draws (`splash_show_embedded`, `splash_apply_override`) in `splash_draw.cpp`; a generated, committed embedded asset (`splash_data.cpp`) produced by `tools/mksplash.py`; boot reordered so `Display` comes up before USB/SD and draws the splash, which the browser's first render dismisses.

**Tech Stack:** C++ (bare-metal, Circle), GNU make, Python 3 (dev-only converter; placeholder path needs no Pillow). Host unit tests in `test/`. Kernel cross-compiles to `kernel7.img` via repo-root `make`.

## Global Constraints

- Pure code (`src/video/splash.cpp`) has **no Circle dependency** — it's compiled by `test/test_splash.cpp`. Circle draws live in a separate `splash_draw.cpp`.
- Asset format `SGSP`: bytes `'S','G','S','P'`, then `u16 width` LE, `u16 height` LE, then `width*height` RGB565 pixels LE. Total length `8 + w*h*2`. Used by the **SD file**; the embedded asset exposes `g_splash_w/g_splash_h/g_splash_data` directly (no header).
- Embedded image kept modest (~160×100) to stay under the `KERNEL_MAX_SIZE` ceiling.
- Logo only — black background, centered (via `blit_rgb565`), integer-scaled by `splash_scale` (~70% fit). No text, no animation, no minimum display time.
- Generated `splash_data.cpp` is committed (no build-time Python dependency).
- SD override path: `SD:/splash.raw`. Absent/unreadable/malformed → keep embedded.

---

### Task 1: Pure helpers — `splash_parse` + `splash_scale` (host-tested)

**Files:**
- Create: `src/video/splash.h`
- Create: `src/video/splash.cpp`
- Test: `test/test_splash.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Produces:
  - `struct SplashImage { unsigned w; unsigned h; const unsigned short *pixels; };`
  - `bool splash_parse(const unsigned char *buf, size_t len, SplashImage *out);`
  - `unsigned splash_scale(unsigned fbW, unsigned fbH, unsigned imgW, unsigned imgH);`
  - (also forward-declares the Circle draw functions used in Task 3/4)

- [ ] **Step 1: Create the header `src/video/splash.h`**

```cpp
//
// src/video/splash.h
//
// Bare Metal Sega Genesis
// Boot splash: pure SGSP-buffer parse + integer-scale helpers (host-testable),
// plus Circle-side draws (defined in splash_draw.cpp).
//

#ifndef _video_splash_h
#define _video_splash_h

#include <stddef.h>

// A parsed view over an SGSP buffer; pixels point INTO the caller's buffer.
struct SplashImage { unsigned w; unsigned h; const unsigned short *pixels; };

// Validate an SGSP buffer: magic 'SGSP', non-zero dims, and exactly
// 8 + w*h*2 bytes. On success fills out and returns true; else returns false.
bool splash_parse(const unsigned char *buf, size_t len, SplashImage *out);

// Largest integer scale with imgW*scale <= fbW*7/10 AND imgH*scale <= fbH*7/10
// (minimum 1; returns 1 if either image dimension is 0).
unsigned splash_scale(unsigned fbW, unsigned fbH, unsigned imgW, unsigned imgH);

// Circle-side draws (splash_draw.cpp). Forward-declared dependencies.
class TextCanvas;
class Display;
class Storage;

// Clear to black and blit the EMBEDDED logo centered, integer-scaled.
void splash_show_embedded(TextCanvas *canvas, Display *display);
// If SD:/splash.raw parses as a valid SGSP image, clear + blit it instead.
void splash_apply_override(Storage *storage, TextCanvas *canvas, Display *display);

#endif
```

- [ ] **Step 2: Write the failing test `test/test_splash.cpp`**

```cpp
#include "../src/video/splash.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    // Valid 2x2 SGSP buffer.
    unsigned char b[8 + 2 * 2 * 2];
    memcpy(b, "SGSP", 4);
    b[4] = 2; b[5] = 0; b[6] = 2; b[7] = 0;       // w=2, h=2 (LE)
    for (int i = 0; i < 8; i++) b[8 + i] = (unsigned char) i;

    SplashImage img;
    assert(splash_parse(b, sizeof b, &img));
    assert(img.w == 2 && img.h == 2);
    assert(img.pixels == (const unsigned short *) (b + 8));

    // Bad magic -> reject.
    unsigned char bad[sizeof b]; memcpy(bad, b, sizeof b); bad[0] = 'X';
    assert(!splash_parse(bad, sizeof bad, &img));

    // Wrong length (one byte short) -> reject.
    assert(!splash_parse(b, sizeof b - 1, &img));
    // Shorter than the 8-byte header -> reject.
    assert(!splash_parse(b, 4, &img));

    // Zero dimensions -> reject.
    unsigned char z[8]; memcpy(z, "SGSP", 4);
    z[4] = z[5] = z[6] = z[7] = 0;
    assert(!splash_parse(z, sizeof z, &img));

    // splash_scale: 1280x720 fb, 160x100 img -> 5 (6 would exceed 70%).
    assert(splash_scale(1280, 720, 160, 100) == 5);
    // Image bigger than 70% even at scale 1 -> 1.
    assert(splash_scale(100, 100, 200, 200) == 1);
    // Tiny image on large fb -> at least 1.
    assert(splash_scale(1920, 1080, 16, 16) >= 1);
    // Zero image dimension -> 1.
    assert(splash_scale(1280, 720, 0, 100) == 1);

    printf("All splash tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the `test_splash` target to `test/Makefile`**

Add `test_splash` to the `run:` dependency list and its `./test_splash` invocation, add the build rule, and add `test_splash` to the `clean:` `rm -f` list:

```make
test_splash: test_splash.cpp ../src/video/splash.cpp ../src/video/splash.h
	$(CXX) $(CXXFLAGS) -o $@ test_splash.cpp ../src/video/splash.cpp
```

- [ ] **Step 4: Run the test to verify it fails (does not link)**

Run: `cd test && make test_splash`
Expected: FAIL — `undefined reference to splash_parse` / `splash_scale` (header exists, no impl yet).

- [ ] **Step 5: Implement the pure helpers in `src/video/splash.cpp`**

```cpp
//
// src/video/splash.cpp
//
// Bare Metal Sega Genesis
// See splash.h — pure SGSP parse + integer-scale helpers (no Circle deps).
//

#include "splash.h"

bool splash_parse(const unsigned char *buf, size_t len, SplashImage *out)
{
    if (buf == 0 || len < 8) return false;
    if (buf[0] != 'S' || buf[1] != 'G' || buf[2] != 'S' || buf[3] != 'P')
        return false;
    unsigned w = (unsigned) buf[4] | ((unsigned) buf[5] << 8);
    unsigned h = (unsigned) buf[6] | ((unsigned) buf[7] << 8);
    if (w == 0 || h == 0) return false;
    if (len != (size_t) 8 + (size_t) w * h * 2) return false;
    out->w = w;
    out->h = h;
    out->pixels = (const unsigned short *) (buf + 8);
    return true;
}

unsigned splash_scale(unsigned fbW, unsigned fbH, unsigned imgW, unsigned imgH)
{
    if (imgW == 0 || imgH == 0) return 1;
    unsigned maxW = fbW * 7 / 10;
    unsigned maxH = fbH * 7 / 10;
    unsigned s = 1;
    while (imgW * (s + 1) <= maxW && imgH * (s + 1) <= maxH) s++;
    return s;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd test && make test_splash && ./test_splash`
Expected: PASS — `All splash tests passed`.

- [ ] **Step 7: Run the full host suite (no regressions)**

Run: `cd test && make && ./test_splash && ./test_blit && ./test_settings`
Expected: `All splash tests passed`, `All blit tests passed`, `All settings tests passed`.

- [ ] **Step 8: Commit**

```bash
git add src/video/splash.h src/video/splash.cpp test/test_splash.cpp test/Makefile
git commit -m "Splash: pure SGSP parse + integer-scale helpers + host test

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Converter tool + embedded placeholder asset

**Files:**
- Create: `tools/mksplash.py`
- Create: `src/video/splash_data.h`
- Create (generated, committed): `src/video/splash_data.cpp`

**Interfaces:**
- Produces:
  - `extern const unsigned g_splash_w, g_splash_h;`
  - `extern const unsigned short g_splash_data[];` (length `g_splash_w*g_splash_h`)
  - `tools/mksplash.py` with `--placeholder`, `--cpp <out>`, `--raw <out>`

- [ ] **Step 1: Create `src/video/splash_data.h`**

```cpp
//
// src/video/splash_data.h
//
// Bare Metal Sega Genesis
// Embedded boot-splash logo (generated by tools/mksplash.py — do not edit the
// generated splash_data.cpp by hand). RGB565, g_splash_w * g_splash_h pixels.
//

#ifndef _video_splash_data_h
#define _video_splash_data_h

extern const unsigned       g_splash_w;
extern const unsigned       g_splash_h;
extern const unsigned short g_splash_data[];

#endif
```

- [ ] **Step 2: Create the converter `tools/mksplash.py`**

```python
#!/usr/bin/env python3
"""Convert a PNG (or a procedural placeholder) to the boot-splash asset.

Usage:
  mksplash.py --placeholder --cpp src/video/splash_data.cpp
  mksplash.py logo.png      --cpp src/video/splash_data.cpp
  mksplash.py --placeholder --raw splash.raw
  mksplash.py logo.png      --raw splash.raw

PNG input requires Pillow; --placeholder needs nothing but the stdlib.
"""
import sys
import struct
import argparse


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def placeholder(w=160, h=100):
    """Dark field with three colored vertical bars (red/blue/yellow)."""
    bg = (24, 24, 32)
    bars = [(220, 40, 40), (40, 90, 220), (230, 200, 40)]
    bar_w, gap = 24, 12
    total = len(bars) * bar_w + (len(bars) - 1) * gap
    x0 = (w - total) // 2
    y0, y1 = h * 20 // 100, h * 80 // 100
    px = []
    for y in range(h):
        for x in range(w):
            c = bg
            if y0 <= y < y1:
                rel = x - x0
                if 0 <= rel < total:
                    slot = rel // (bar_w + gap)
                    off = rel - slot * (bar_w + gap)
                    if slot < len(bars) and off < bar_w:
                        c = bars[slot]
            px.append(rgb565(*c))
    return w, h, px


def from_png(path):
    from PIL import Image
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = [rgb565(r, g, b) for (r, g, b) in im.getdata()]
    return w, h, px


def write_cpp(path, w, h, px):
    with open(path, "w") as f:
        f.write("//\n// src/video/splash_data.cpp\n//\n")
        f.write("// Generated by tools/mksplash.py - do not edit.\n//\n\n")
        f.write('#include "splash_data.h"\n\n')
        f.write("const unsigned       g_splash_w = %d;\n" % w)
        f.write("const unsigned       g_splash_h = %d;\n" % h)
        f.write("const unsigned short g_splash_data[%d] = {\n" % (w * h))
        for i in range(0, len(px), 12):
            row = ",".join("0x%04x" % v for v in px[i:i + 12])
            f.write("    " + row + ",\n")
        f.write("};\n")


def write_raw(path, w, h, px):
    with open(path, "wb") as f:
        f.write(b"SGSP")
        f.write(struct.pack("<HH", w, h))
        for v in px:
            f.write(struct.pack("<H", v))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("png", nargs="?")
    ap.add_argument("--placeholder", action="store_true")
    ap.add_argument("--cpp")
    ap.add_argument("--raw")
    a = ap.parse_args()
    if a.placeholder:
        w, h, px = placeholder()
    elif a.png:
        w, h, px = from_png(a.png)
    else:
        ap.error("give a PNG or --placeholder")
    if not a.cpp and not a.raw:
        ap.error("give --cpp and/or --raw")
    if a.cpp:
        write_cpp(a.cpp, w, h, px)
    if a.raw:
        write_raw(a.raw, w, h, px)
    print("splash %dx%d -> %s" % (w, h, a.cpp or a.raw))


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Generate the committed placeholder asset**

Run: `python3 tools/mksplash.py --placeholder --cpp src/video/splash_data.cpp`
Expected: prints `splash 160x100 -> src/video/splash_data.cpp`, and `src/video/splash_data.cpp` now exists with `g_splash_w = 160`, `g_splash_h = 100`.

- [ ] **Step 4: Verify the generated asset is self-consistent and compiles**

Run:
```bash
grep -c "0x" src/video/splash_data.cpp >/dev/null && \
python3 -c "import re;d=open('src/video/splash_data.cpp').read();n=len(re.findall(r'0x[0-9a-f]{4}',d));assert n==160*100, n;print('pixel count', n, 'OK')" && \
g++ -fsyntax-only -I src/video src/video/splash_data.cpp && echo "compiles OK"
```
Expected: `pixel count 16000 OK` then `compiles OK`.

- [ ] **Step 5: Commit**

```bash
chmod +x tools/mksplash.py
git add tools/mksplash.py src/video/splash_data.h src/video/splash_data.cpp
git commit -m "Splash: mksplash.py converter + committed placeholder asset (160x100)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Circle draw functions + build wiring

**Files:**
- Create: `src/video/splash_draw.cpp`
- Modify: `Makefile` (OBJS list, near `src/video/display.o`)

**Interfaces:**
- Consumes: `splash_parse`, `splash_scale`, `SplashImage` (Task 1); `g_splash_data/w/h` (Task 2); `blit_rgb565` (`blit.h`); `Display::Buffer/Pitch/Width/Height`; `TextCanvas::Clear`; `Storage::Exists/ReadFile`.
- Produces: defined `splash_show_embedded` / `splash_apply_override`.

- [ ] **Step 1: Create `src/video/splash_draw.cpp`**

```cpp
//
// src/video/splash_draw.cpp
//
// Bare Metal Sega Genesis
// Circle-side boot-splash draws: blit the embedded (or SD-override) logo
// centered on a black framebuffer. See splash.h.
//

#include "splash.h"
#include "splash_data.h"
#include "display.h"
#include "blit.h"
#include "../ui/text_canvas.h"
#include "../storage/storage.h"

// Clear to black and blit a w*h RGB565 image centered, integer-scaled.
static void draw_image(TextCanvas *canvas, Display *display,
                       const unsigned short *pixels, unsigned w, unsigned h)
{
    if (display->Buffer() == 0) return;
    canvas->Clear(0x0000);
    unsigned s = splash_scale(display->Width(), display->Height(), w, h);
    blit_rgb565((unsigned short *) display->Buffer(), display->Pitch(),
                display->Width(), display->Height(),
                pixels, w * 2, w, h, s);   // blit_rgb565 centers the image
}

void splash_show_embedded(TextCanvas *canvas, Display *display)
{
    draw_image(canvas, display, g_splash_data, g_splash_w, g_splash_h);
}

void splash_apply_override(Storage *storage, TextCanvas *canvas, Display *display)
{
    const char *path = "SD:/splash.raw";
    if (!storage->Exists(path)) return;

    u8    *buf  = 0;
    size_t size = 0;
    if (!storage->ReadFile(path, &buf, &size)) return;   // ReadFile new[]s buf

    SplashImage img;
    if (splash_parse(buf, size, &img))
        draw_image(canvas, display, img.pixels, img.w, img.h);

    delete[] buf;
}
```

(`Display::Buffer()` returns `u16 *`; the cast to `unsigned short *` is a no-op on this target where `u16 == unsigned short`, and matches `blit_rgb565`'s `uint16_t *`.)

- [ ] **Step 2: Add the new objects to `Makefile` OBJS**

After the `src/video/display.o \` line in the `OBJS =` list, add:

```make
       src/video/splash.o \
       src/video/splash_draw.o \
       src/video/splash_data.o \
```

- [ ] **Step 3: Build the kernel (functions compile + link, not yet called)**

Run: `make` (from the repo root)
Expected: compiles `splash.o`, `splash_draw.o`, `splash_data.o`, links `kernel7.img`, no errors.

- [ ] **Step 4: Commit**

```bash
git add src/video/splash_draw.cpp Makefile
git commit -m "Splash: Circle draw funcs (embedded + SD override) + build wiring

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Boot integration + hardware checklist

**Files:**
- Modify: `src/kernel.cpp` (add include; reorder Display before USB; call splash after Display init and after SD mount)
- Modify: `docs/hardware-verification-checklist-2026-06-20.md` (append a section)

**Interfaces:**
- Consumes: `splash_show_embedded`, `splash_apply_override` (Task 3); existing `m_Display`, `m_Canvas`, `m_Storage`.

- [ ] **Step 1: Include the splash header in `src/kernel.cpp`**

After the existing `#include "audio/audio_util.h"` line near the top, add:

```cpp
#include "video/splash.h"       // splash_show_embedded, splash_apply_override
```

- [ ] **Step 2: Reorder Display init before USB, and show the embedded splash**

In `CKernel::Initialize`, the current order is the USB block, then the SD card block, then the video block. Replace those three `if (bOK) { ... }` blocks (the USB `m_USBHCI.Initialize()`, the SD `m_EMMC.Initialize()`, and the video `m_Display.Initialize()` blocks) with the video block FIRST, immediately drawing the splash, then USB, then SD:

```cpp
	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising video");
		bOK = m_Display.Initialize ();
		if (!bOK)
		{
			m_Logger.Write (FromKernel, LogPanic, "Display init failed");
		}
		else
		{
			// Branded splash up ASAP, masking the USB/SD init that follows.
			splash_show_embedded (&m_Canvas, &m_Display);
		}
	}

	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising USB");
		bOK = m_USBHCI.Initialize ();
	}

	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising SD card");
		bOK = m_EMMC.Initialize ();
	}
```

- [ ] **Step 3: Apply the SD override after the card mounts**

In `CKernel::Run`, immediately after the successful `m_Storage.Mount()` check (the block that `return ShutdownHalt;` on failure), before `m_SettingsStore.Load (&m_Settings);`, add:

```cpp
	// Replace the embedded logo with SD:/splash.raw if the user supplied one.
	splash_apply_override (&m_Storage, &m_Canvas, &m_Display);
```

- [ ] **Step 4: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion, `kernel7.img` produced, no errors.

- [ ] **Step 5: Append a hardware-verify checklist section**

In `docs/hardware-verification-checklist-2026-06-20.md`, before the `## Results summary` section, add:

```markdown
## Q. Boot splash screen

- [ ] **Q1 — Embedded logo on boot.** Power on. **Expect:** the placeholder logo
  (three colored bars on a dark field) appears early and stays through the USB/SD
  wait, then the ROM browser replaces it. No corruption or hang.
- [ ] **Q2 — SD override.** Make `splash.raw` with
  `python3 tools/mksplash.py --placeholder --raw splash.raw` (or from a PNG),
  copy it to the SD root, reboot. **Expect:** after the card mounts, that image
  replaces the embedded logo for the rest of boot.
- [ ] **Q3 — Bad/absent override is harmless.** Remove or truncate
  `SD:/splash.raw`, reboot. **Expect:** the embedded logo shows for the whole
  boot; no hang.
```

Also add to the results-summary table (after the last row):

```markdown
| Q. Boot splash | | |
```

- [ ] **Step 6: Commit**

```bash
git add src/kernel.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Kernel: show boot splash early (Display before USB) + SD override

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes for the implementer

- Host tests build natively in `test/`: `cd test && make`.
- The kernel build (`make` at repo root) is the only verification for the Circle
  draws and the boot reorder — no host test covers `splash_draw.cpp` or `kernel.cpp`,
  consistent with the rest of the project.
- `Display::Initialize()` uses only the VideoCore mailbox, so moving it before
  USB/EMMC is safe. Console `Logger.Write` during the subsequent init does not
  corrupt the live `Display` framebuffer (the gameplay loop already proves this).
- Do not hand-edit `src/video/splash_data.cpp`; regenerate it with `mksplash.py`.
```
