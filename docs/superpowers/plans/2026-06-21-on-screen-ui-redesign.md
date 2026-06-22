# On-Screen UI Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-skin the ROM browser (pilot) to the Claude Design v1.0.0 look using a new pixel-font renderer, building the reusable engine (fonts, blend, scanlines, theme) that the other 8 screens will later adopt.

**Architecture:** Pure, host-testable raster primitives (`pixel_ops`, `glyph_draw`, `icons`, `rom_layout`) operate on a raw RGB565 buffer with no Circle dependency. A thin Circle wrapper (`GlyphCanvas`) binds them to `Display`. Custom bitmap fonts are generated offline by `tools/mkfont.py` into committed C source. `theme.h` centralizes the palette. The pilot screen `rom_menu` is rewired from `TextCanvas` to `GlyphCanvas`; `TextCanvas` stays for the un-migrated screens.

**Tech Stack:** C++ (bare-metal, Circle framework, AArch32 / Pi 2), RGB565 framebuffer, Python 3 + Pillow (host font tool), host C++ unit tests (system compiler + `assert`).

## Global Constraints

- **Target:** Raspberry Pi 2, AArch32; produces `kernel7.img`. Toolchain prefix `arm-linux-gnueabihf-`. (Kernel cross-build: `make` at repo root.)
- **Pixel format:** RGB565, 16-bit. Framebuffer = full HDMI output resolution (e.g. 1920×1080); UI draws into `Display::Buffer()` (the visible front page).
- **Host tests:** live in `test/`, compiled with the system `c++` (NOT the cross toolchain), plain `assert` + `printf("... OK\n")`, one Makefile target each, all run by `make -C test run`.
- **No Circle in pure modules:** `pixel_ops`, `glyph_draw`, `icons`, `rom_layout`, `font.h`, `theme.h` must compile host-side with only `<stdint.h>`/`<string.h>` — never include `circle/*` or `storage.h` (which pulls Circle).
- **Palette (RGB888 → RGB565), from the mock:** bg `#0a0b10`, selection `#E11B22`, value/info `#54CBE6`, adjust `#F5B824`, active `#45C26A`, body `#ECECE4`, muted `#8a93a0`, dim `#69737f`.
- **Out of scope:** gradients, blurred/neon glow, runtime accent picker, animated easing, the spec-sheet bezel/grid/changelog chrome.
- **Fonts:** Press Start 2P (native 8px base, integer-scaled at draw time) + VT323 (baked at ~16px and ~22px). OFL fonts; license text committed under `tools/fonts/`.
- **Kernel sources** are listed explicitly in root `Makefile` `OBJS = …`; every new `src/**/*.cpp` must be added there.

---

### Task 1: RGB565 alpha blend (`pixel_ops`)

**Files:**
- Create: `src/ui/pixel_ops.h`, `src/ui/pixel_ops.cpp`
- Test: `test/test_pixel_ops.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Produces: `uint16_t rgb565_blend(uint16_t dst, uint16_t src, uint8_t alpha)` — alpha 0 ⇒ `dst`, 255 ⇒ `src`, linear per channel.

- [ ] **Step 1: Write the failing test**

`test/test_pixel_ops.cpp`:
```cpp
#include "../src/ui/pixel_ops.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    // alpha extremes return the endpoints exactly
    assert(rgb565_blend(0x1234, 0xABCD, 0)   == 0x1234);
    assert(rgb565_blend(0x1234, 0xABCD, 255) == 0xABCD);

    // blending a color toward black at ~50% halves each channel (approx)
    // src = pure red 0xF800 (R=31), dst = black. alpha 128 -> R ~= 15..16
    uint16_t r = rgb565_blend(0x0000, 0xF800, 128);
    unsigned R = (r >> 11) & 0x1F;
    assert(R >= 15 && R <= 16);
    assert(((r >> 5) & 0x3F) == 0);
    assert((r & 0x1F) == 0);

    // blend is monotonic in alpha for a single channel
    uint16_t a = rgb565_blend(0x0000, 0x001F, 64);
    uint16_t b = rgb565_blend(0x0000, 0x001F, 192);
    assert((a & 0x1F) < (b & 0x1F));

    printf("test_pixel_ops OK\n");
    return 0;
}
```

- [ ] **Step 2: Add the Makefile target and run to verify it fails**

In `test/Makefile`, add `test_pixel_ops` to the `run:` target list (after `test_blit`) and its run line, plus a build rule:
```make
test_pixel_ops: test_pixel_ops.cpp ../src/ui/pixel_ops.cpp ../src/ui/pixel_ops.h
	$(CXX) $(CXXFLAGS) -o $@ test_pixel_ops.cpp ../src/ui/pixel_ops.cpp
```
Also add `test_pixel_ops` to the `clean:` `rm -f` list.

Run: `make -C test test_pixel_ops`
Expected: FAIL — `pixel_ops.h: No such file` / link error (no `rgb565_blend`).

- [ ] **Step 3: Write the implementation**

`src/ui/pixel_ops.h`:
```cpp
//
// src/ui/pixel_ops.h
//
// Bare Metal Sega Genesis
// Pure RGB565 pixel math (host-testable, no Circle deps).
//
#ifndef _ui_pixel_ops_h
#define _ui_pixel_ops_h
#include <stdint.h>

// Blend src over dst in RGB565. alpha: 0 => dst unchanged, 255 => src fully.
uint16_t rgb565_blend(uint16_t dst, uint16_t src, uint8_t alpha);

#endif
```

`src/ui/pixel_ops.cpp`:
```cpp
//
// src/ui/pixel_ops.cpp
//
// Bare Metal Sega Genesis
// See pixel_ops.h.
//
#include "pixel_ops.h"

uint16_t rgb565_blend(uint16_t dst, uint16_t src, uint8_t alpha) {
    unsigned a = alpha;
    unsigned ia = 255u - a;

    unsigned dr = (dst >> 11) & 0x1F, dg = (dst >> 5) & 0x3F, db = dst & 0x1F;
    unsigned sr = (src >> 11) & 0x1F, sg = (src >> 5) & 0x3F, sb = src & 0x1F;

    unsigned r = (sr * a + dr * ia + 127) / 255;
    unsigned g = (sg * a + dg * ia + 127) / 255;
    unsigned b = (sb * a + db * ia + 127) / 255;

    return (uint16_t) ((r << 11) | (g << 5) | b);
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `make -C test test_pixel_ops && ./test/test_pixel_ops`
Expected: `test_pixel_ops OK`

- [ ] **Step 5: Commit**
```bash
git add src/ui/pixel_ops.h src/ui/pixel_ops.cpp test/test_pixel_ops.cpp test/Makefile
git commit -m "feat(ui): RGB565 alpha blend primitive (pixel_ops)"
```

---

### Task 2: Font format + pure rasterizer (`font.h`, `glyph_draw`)

**Files:**
- Create: `src/ui/font.h`, `src/ui/glyph_draw.h`, `src/ui/glyph_draw.cpp`
- Test: `test/test_glyph_draw.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Consumes: `rgb565_blend` (Task 1).
- Produces:
  - `struct Font { uint8_t first, last, width, height, stride; const uint8_t *bitmap; };` (in `font.h`). `stride = (width + 7) / 8`; bitmap is `(last-first+1) * height * stride` bytes, row-major, MSB-first per row.
  - `void gd_fill_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int w, int h, uint16_t color);`
  - `void gd_blend_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int w, int h, uint16_t color, uint8_t alpha);`
  - `void gd_scanlines(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int w, int h, uint8_t strength);`
  - `int gd_text_width(const Font *f, int scale, const char *s);`
  - `int gd_draw_text(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, const Font *f, int scale, int x, int y, const char *s, uint16_t fg, uint16_t bg, bool transparent);` (returns pen x after last glyph)

- [ ] **Step 1: Write the failing test**

`test/test_glyph_draw.cpp`:
```cpp
#include "../src/ui/glyph_draw.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define W 32u
#define H 16u
static uint16_t fb[W * H];

// 3x5 font holding two glyphs: '!' (full column) at code 0x21 and ' ' (blank)
// at 0x20. stride=1. Rows are MSB-first; we use the top 3 bits.
//  ' ' : all rows 0
//  '!' : a vertical bar in the middle column -> 0b010xxxxx = 0x40
static const uint8_t kBitmap[] = {
    0x00,0x00,0x00,0x00,0x00,   // 0x20 ' '
    0x40,0x40,0x40,0x40,0x40,   // 0x21 '!'
};
static const Font kFont = { 0x20, 0x21, 3, 5, 1, kBitmap };

static void reset(void){ memset(fb, 0, sizeof(fb)); }

int main(void) {
    // fill_rect writes a solid block, clipped to the buffer
    reset();
    gd_fill_rect(fb, W, W, H, 2, 3, 4, 2, 0xBEEF);
    assert(fb[3 * W + 2] == 0xBEEF);
    assert(fb[4 * W + 5] == 0xBEEF);
    assert(fb[3 * W + 6] == 0x0000);   // just outside the width
    // negative origin is clipped, no crash, no out-of-bounds write
    gd_fill_rect(fb, W, W, H, -4, -4, 6, 6, 0x1111);
    assert(fb[0] == 0x1111);

    // blend_rect at alpha 255 == solid
    reset();
    gd_blend_rect(fb, W, W, H, 0, 0, 3, 3, 0x07E0, 255);
    assert(fb[0] == 0x07E0);

    // scanlines darken every 3rd row (y % 3 == 2) within the region only
    reset();
    gd_fill_rect(fb, W, W, H, 0, 0, 8, 6, 0xFFFF);
    gd_scanlines(fb, W, W, H, 0, 0, 8, 6, 128);
    assert(fb[0 * W + 0] == 0xFFFF);          // row 0 untouched
    assert(fb[2 * W + 0] != 0xFFFF);          // row 2 darkened
    assert(fb[2 * W + 0] != 0x0000);          // but not fully black

    // text width = chars * width * scale
    assert(gd_text_width(&kFont, 1, "!!") == 6);
    assert(gd_text_width(&kFont, 2, "!")  == 6);

    // draw '!' at scale 1: middle column set to fg, others bg
    reset();
    int penx = gd_draw_text(fb, W, W, H, &kFont, 1, 0, 0, "!", 0xF800, 0x001F, false);
    assert(penx == 3);
    assert(fb[0 * W + 1] == 0xF800);          // middle column, glyph on
    assert(fb[0 * W + 0] == 0x001F);          // left column, bg drawn
    // transparent mode leaves bg pixels as they were
    reset();
    fb[0 * W + 0] = 0x1234;
    gd_draw_text(fb, W, W, H, &kFont, 1, 0, 0, "!", 0xF800, 0x001F, true);
    assert(fb[0 * W + 0] == 0x1234);          // untouched (glyph off here)
    assert(fb[0 * W + 1] == 0xF800);          // glyph on

    // scale 2 makes each on-pixel a 2x2 block
    reset();
    gd_draw_text(fb, W, W, H, &kFont, 2, 0, 0, "!", 0xF800, 0x0000, false);
    assert(fb[0 * W + 2] == 0xF800 && fb[1 * W + 3] == 0xF800);

    printf("test_glyph_draw OK\n");
    return 0;
}
```

- [ ] **Step 2: Add the Makefile target and run to verify it fails**

In `test/Makefile` add `test_glyph_draw` to `run:` (deps + run line) and `clean:`, with rule:
```make
test_glyph_draw: test_glyph_draw.cpp ../src/ui/glyph_draw.cpp ../src/ui/glyph_draw.h ../src/ui/font.h ../src/ui/pixel_ops.cpp ../src/ui/pixel_ops.h
	$(CXX) $(CXXFLAGS) -o $@ test_glyph_draw.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp
```
Run: `make -C test test_glyph_draw`
Expected: FAIL — missing `glyph_draw.h`.

- [ ] **Step 3: Write `font.h`**

`src/ui/font.h`:
```cpp
//
// src/ui/font.h
//
// Bare Metal Sega Genesis
// Fixed-width bitmap font format shared by the rasterizer and generated fonts.
// Pure data; no Circle deps.
//
#ifndef _ui_font_h
#define _ui_font_h
#include <stdint.h>

struct Font {
    uint8_t first;          // first code point (e.g. 0x20)
    uint8_t last;           // last code point  (e.g. 0x7E)
    uint8_t width;          // glyph width  in px
    uint8_t height;         // glyph height in px
    uint8_t stride;         // bytes per glyph row = (width + 7) / 8
    const uint8_t *bitmap;  // (last-first+1)*height*stride bytes, MSB-first per row
};

#endif
```

- [ ] **Step 4: Write `glyph_draw.h`**

`src/ui/glyph_draw.h`:
```cpp
//
// src/ui/glyph_draw.h
//
// Bare Metal Sega Genesis
// Pure RGB565 raster primitives over a raw framebuffer (no Circle deps).
// pitchPx is the row stride in PIXELS (Display::Pitch()/2). All ops clip.
//
#ifndef _ui_glyph_draw_h
#define _ui_glyph_draw_h
#include <stdint.h>
#include "font.h"

void gd_fill_rect (uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                   int x, int y, int w, int h, uint16_t color);
void gd_blend_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                   int x, int y, int w, int h, uint16_t color, uint8_t alpha);
void gd_scanlines (uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                   int x, int y, int w, int h, uint8_t strength);

int  gd_text_width(const Font *f, int scale, const char *s);

// Draws s with integer scale (>=1). When transparent, off pixels are left
// untouched (bg ignored). Returns pen x after the last glyph.
int  gd_draw_text (uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                   const Font *f, int scale, int x, int y, const char *s,
                   uint16_t fg, uint16_t bg, bool transparent);

#endif
```

- [ ] **Step 5: Write `glyph_draw.cpp`**

`src/ui/glyph_draw.cpp`:
```cpp
//
// src/ui/glyph_draw.cpp
//
// Bare Metal Sega Genesis
// See glyph_draw.h.
//
#include "glyph_draw.h"
#include "pixel_ops.h"
#include <string.h>

static inline void put(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                       int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= fbw || y >= fbh) return;
    buf[(unsigned) y * pitchPx + (unsigned) x] = c;
}

void gd_fill_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                  int x, int y, int w, int h, uint16_t color) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            put(buf, pitchPx, fbw, fbh, xx, yy, color);
}

void gd_blend_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                   int x, int y, int w, int h, uint16_t color, uint8_t alpha) {
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= fbh) continue;
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || xx >= fbw) continue;
            uint16_t *p = &buf[(unsigned) yy * pitchPx + (unsigned) xx];
            *p = rgb565_blend(*p, color, alpha);
        }
    }
}

void gd_scanlines(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                  int x, int y, int w, int h, uint8_t strength) {
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= fbh) continue;
        if ((yy % 3) != 2) continue;            // darken every 3rd row
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || xx >= fbw) continue;
            uint16_t *p = &buf[(unsigned) yy * pitchPx + (unsigned) xx];
            *p = rgb565_blend(*p, 0x0000, strength);
        }
    }
}

int gd_text_width(const Font *f, int scale, const char *s) {
    if (f == 0 || s == 0) return 0;
    return (int) strlen(s) * (int) f->width * scale;
}

int gd_draw_text(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                 const Font *f, int scale, int x, int y, const char *s,
                 uint16_t fg, uint16_t bg, bool transparent) {
    if (f == 0 || s == 0 || scale < 1) return x;
    int penx = x;
    for (unsigned i = 0; s[i] != '\0'; i++) {
        unsigned char c = (unsigned char) s[i];
        if (c < f->first || c > f->last) c = f->first;   // fall back to first glyph
        const uint8_t *g = f->bitmap +
            (unsigned) (c - f->first) * f->height * f->stride;
        for (unsigned gy = 0; gy < f->height; gy++) {
            const uint8_t *row = g + gy * f->stride;
            for (unsigned gx = 0; gx < f->width; gx++) {
                bool on = (row[gx >> 3] >> (7 - (gx & 7))) & 1;
                if (!on && transparent) continue;
                uint16_t col = on ? fg : bg;
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        put(buf, pitchPx, fbw, fbh,
                            penx + (int) gx * scale + sx,
                            y + (int) gy * scale + sy, col);
            }
        }
        penx += (int) f->width * scale;
    }
    return penx;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test test_glyph_draw && ./test/test_glyph_draw`
Expected: `test_glyph_draw OK`

- [ ] **Step 7: Commit**
```bash
git add src/ui/font.h src/ui/glyph_draw.h src/ui/glyph_draw.cpp test/test_glyph_draw.cpp test/Makefile
git commit -m "feat(ui): font format + pure RGB565 glyph rasterizer (glyph_draw)"
```

---

### Task 3: Icon primitives (`icons`)

**Files:**
- Create: `src/ui/icons.h`, `src/ui/icons.cpp`
- Test: `test/test_icons.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Consumes: `gd_fill_rect`, `gd_draw_text` (Task 2); `Font` (Task 2).
- Produces:
  - `void icon_play(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int size, uint16_t color);` — right-pointing filled triangle in a `size`×`size` box.
  - `void icon_cross(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int size, uint16_t color);` — d-pad plus in a `size`×`size` box.
  - `void icon_button(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int d, char letter, uint16_t fill, uint16_t fg, const Font *f);` — `d`×`d` filled square + centered letter (font `f`, scale 1).

- [ ] **Step 1: Write the failing test**

`test/test_icons.cpp`:
```cpp
#include "../src/ui/icons.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define W 24u
#define H 24u
static uint16_t fb[W * H];
static void reset(void){ memset(fb, 0, sizeof(fb)); }

// reuse the 3x5 '!' font from glyph_draw's test space
static const uint8_t kBitmap[] = {
    0x00,0x00,0x00,0x00,0x00,   // ' '
    0x40,0x40,0x40,0x40,0x40,   // '!'
};
static const Font kFont = { 0x20, 0x21, 3, 5, 1, kBitmap };

int main(void) {
    // play triangle: tip column near the right edge is set; far bottom-right is empty
    reset();
    icon_play(fb, W, W, H, 0, 0, 8, 0xF800);
    assert(fb[4 * W + 0] == 0xF800);          // left edge mid-height = base of triangle
    assert(fb[0 * W + 7] == 0x0000);          // top-right corner empty (tapered)

    // cross: center row and center column are set, corners empty
    reset();
    icon_cross(fb, W, W, H, 0, 0, 9, 0x07E0);
    assert(fb[4 * W + 4] == 0x07E0);          // center
    assert(fb[0 * W + 0] == 0x0000);          // corner empty

    // button: fill present, and at least one fg (letter) pixel inside
    reset();
    icon_button(fb, W, W, H, 0, 0, 11, '!', 0xE000, 0xFFFF, &kFont);
    assert(fb[0 * W + 0] == 0xE000);          // fill corner
    bool sawLetter = false;
    for (unsigned i = 0; i < W * H; i++) if (fb[i] == 0xFFFF) sawLetter = true;
    assert(sawLetter);

    printf("test_icons OK\n");
    return 0;
}
```

- [ ] **Step 2: Add the Makefile target and run to verify it fails**

In `test/Makefile` add `test_icons` to `run:` and `clean:`, with rule:
```make
test_icons: test_icons.cpp ../src/ui/icons.cpp ../src/ui/icons.h ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_icons.cpp ../src/ui/icons.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp
```
Run: `make -C test test_icons`
Expected: FAIL — missing `icons.h`.

- [ ] **Step 3: Write `icons.h`**

`src/ui/icons.h`:
```cpp
//
// src/ui/icons.h
//
// Bare Metal Sega Genesis
// Pure FillRect/glyph-based UI icons for hint bars and selection markers.
//
#ifndef _ui_icons_h
#define _ui_icons_h
#include <stdint.h>
#include "font.h"

void icon_play (uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                int x, int y, int size, uint16_t color);
void icon_cross(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                int x, int y, int size, uint16_t color);
void icon_button(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                 int x, int y, int d, char letter,
                 uint16_t fill, uint16_t fg, const Font *f);

#endif
```

- [ ] **Step 4: Write `icons.cpp`**

`src/ui/icons.cpp`:
```cpp
//
// src/ui/icons.cpp
//
// Bare Metal Sega Genesis
// See icons.h.
//
#include "icons.h"
#include "glyph_draw.h"

void icon_play(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
               int x, int y, int size, uint16_t color) {
    // Right-pointing triangle: width shrinks toward the tip as |row - mid| grows.
    int mid = size / 2;
    for (int r = 0; r < size; r++) {
        int dist = (r < mid) ? r : (size - 1 - r);   // 0 at edges, max at center
        int w = (dist * size) / size;                // taper width = dist
        if (w < 1) w = 1;
        gd_fill_rect(buf, pitchPx, fbw, fbh, x, y + r, w, 1, color);
    }
}

void icon_cross(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                int x, int y, int size, uint16_t color) {
    int t = size / 3;                 // arm thickness
    if (t < 1) t = 1;
    int c = (size - t) / 2;           // arm offset to center
    gd_fill_rect(buf, pitchPx, fbw, fbh, x + c, y,     t, size, color);  // vertical
    gd_fill_rect(buf, pitchPx, fbw, fbh, x, y + c,     size, t, color);  // horizontal
}

void icon_button(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                 int x, int y, int d, char letter,
                 uint16_t fill, uint16_t fg, const Font *f) {
    gd_fill_rect(buf, pitchPx, fbw, fbh, x, y, d, d, fill);
    if (f) {
        char s[2] = { letter, '\0' };
        int tw = gd_text_width(f, 1, s);
        int tx = x + (d - tw) / 2;
        int ty = y + (d - (int) f->height) / 2;
        gd_draw_text(buf, pitchPx, fbw, fbh, f, 1, tx, ty, s, fg, fill, true);
    }
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_icons && ./test/test_icons`
Expected: `test_icons OK`

- [ ] **Step 6: Commit**
```bash
git add src/ui/icons.h src/ui/icons.cpp test/test_icons.cpp test/Makefile
git commit -m "feat(ui): play/cross/button icon primitives"
```

---

### Task 4: Theme palette (`theme.h`)

**Files:**
- Create: `src/ui/theme.h`
- Test: `test/test_theme.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Produces: `theme::BG, SELECTION, VALUE, ADJUST, ACTIVE, TEXT, TEXT_MUTED, TEXT_DIM, WHITE` (`uint16_t` constants), and macro `RGB565(r,g,b)`.

- [ ] **Step 1: Write the failing test**

`test/test_theme.cpp`:
```cpp
#include "../src/ui/theme.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    // RGB565 macro: bg #0a0b10 -> R=1,G=2,B=2 -> 0x0842
    assert(RGB565(0x0a, 0x0b, 0x10) == 0x0842);
    assert(theme::BG == 0x0842);
    // pure white passes through
    assert(RGB565(0xff, 0xff, 0xff) == 0xFFFF);
    assert(theme::WHITE == 0xFFFF);
    // accent red #E11B22 -> R=28,G=6,B=4 -> (28<<11)|(6<<5)|4 = 0xE0C4
    assert(theme::SELECTION == 0xE0C4);
    // distinct roles really are distinct
    assert(theme::VALUE != theme::ADJUST);
    assert(theme::ACTIVE != theme::SELECTION);
    printf("test_theme OK\n");
    return 0;
}
```

- [ ] **Step 2: Add the Makefile target and run to verify it fails**

In `test/Makefile` add `test_theme` to `run:` and `clean:`, with rule:
```make
test_theme: test_theme.cpp ../src/ui/theme.h
	$(CXX) $(CXXFLAGS) -o $@ test_theme.cpp
```
Run: `make -C test test_theme`
Expected: FAIL — missing `theme.h`.

- [ ] **Step 3: Write `theme.h`**

`src/ui/theme.h`:
```cpp
//
// src/ui/theme.h
//
// Bare Metal Sega Genesis
// Central UI palette (from Claude Design v1.0.0). RGB565 constants + semantic
// roles, so screens never hardcode raw hex. Pure header; no Circle deps.
//
#ifndef _ui_theme_h
#define _ui_theme_h
#include <stdint.h>

#define RGB565(r,g,b) ((uint16_t)(((((r) >> 3) & 0x1F) << 11) | \
                                  ((((g) >> 2) & 0x3F) << 5)  | \
                                  (((b) >> 3) & 0x1F)))

namespace theme {
    static const uint16_t BG         = RGB565(0x0a, 0x0b, 0x10);
    static const uint16_t SELECTION  = RGB565(0xE1, 0x1B, 0x22);  // accent red
    static const uint16_t VALUE      = RGB565(0x54, 0xCB, 0xE6);  // cyan
    static const uint16_t ADJUST     = RGB565(0xF5, 0xB8, 0x24);  // amber
    static const uint16_t ACTIVE     = RGB565(0x45, 0xC2, 0x6A);  // green
    static const uint16_t TEXT       = RGB565(0xEC, 0xEC, 0xE4);  // body
    static const uint16_t TEXT_MUTED = RGB565(0x8a, 0x93, 0xa0);
    static const uint16_t TEXT_DIM   = RGB565(0x69, 0x73, 0x7f);
    static const uint16_t WHITE      = 0xFFFF;
}

#endif
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `make -C test test_theme && ./test/test_theme`
Expected: `test_theme OK`

- [ ] **Step 5: Commit**
```bash
git add src/ui/theme.h test/test_theme.cpp test/Makefile
git commit -m "feat(ui): central theme palette (theme.h)"
```

---

### Task 5: Font generation tool + baked fonts (`mkfont.py`)

**Files:**
- Create: `tools/mkfont.py`
- Create (fetched, committed): `tools/fonts/PressStart2P-Regular.ttf`, `tools/fonts/VT323-Regular.ttf`, `tools/fonts/OFL-PressStart2P.txt`, `tools/fonts/OFL-VT323.txt`
- Create (generated, committed): `src/ui/fonts/font_ps2p8.h`, `font_ps2p8.cpp`, `font_vt323_16.h`, `font_vt323_16.cpp`, `font_vt323_22.h`, `font_vt323_22.cpp`
- Test: `test/test_fonts.cpp`
- Modify: `test/Makefile`, root `Makefile` (add a `fonts` phony target — optional convenience)

**Interfaces:**
- Produces: `extern const Font g_font_ps2p8;`, `extern const Font g_font_vt323_16;`, `extern const Font g_font_vt323_22;` (each in its `.h`, defined in its `.cpp`). All cover ASCII `0x20`–`0x7E`.

- [ ] **Step 1: Fetch the OFL fonts**

```bash
mkdir -p tools/fonts src/ui/fonts
curl -L -o tools/fonts/PressStart2P-Regular.ttf \
  https://github.com/google/fonts/raw/main/ofl/pressstart2p/PressStart2P-Regular.ttf
curl -L -o tools/fonts/VT323-Regular.ttf \
  https://github.com/google/fonts/raw/main/ofl/vt323/VT323-Regular.ttf
curl -L -o tools/fonts/OFL-PressStart2P.txt \
  https://github.com/google/fonts/raw/main/ofl/pressstart2p/OFL.txt
curl -L -o tools/fonts/OFL-VT323.txt \
  https://github.com/google/fonts/raw/main/ofl/vt323/OFL.txt
```
Verify both `.ttf` files are non-trivial: `ls -l tools/fonts/*.ttf` (each should be tens of KB).
If the environment is offline, obtain these four files by other means before continuing — the tool needs the two `.ttf`s present.

- [ ] **Step 2: Write `tools/mkfont.py`**

`tools/mkfont.py`:
```python
#!/usr/bin/env python3
"""Render a TTF into a Bare-Metal-Genesis bitmap Font (src/ui/font.h format).

Usage: mkfont.py <ttf> <px_size> <symbol> <out_basename_no_ext>
  e.g. mkfont.py tools/fonts/PressStart2P-Regular.ttf 8 g_font_ps2p8 src/ui/fonts/font_ps2p8
Emits <out>.h and <out>.cpp. Covers ASCII 0x20..0x7E, fixed-width cells.
Requires Pillow (pip install pillow).
"""
import sys
from PIL import Image, ImageFont, ImageDraw

FIRST, LAST = 0x20, 0x7E

def main():
    ttf, px, symbol, out = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    font = ImageFont.truetype(ttf, px)

    # Fixed cell: max advance width over the range, and the px size as height.
    width = 0
    for cp in range(FIRST, LAST + 1):
        box = font.getbbox(chr(cp))
        adv = box[2] - box[0] if box else 0
        width = max(width, adv)
    width = max(width, 1)
    height = px
    stride = (width + 7) // 8

    rows = []
    for cp in range(FIRST, LAST + 1):
        img = Image.new("L", (width, height), 0)
        d = ImageDraw.Draw(img)
        # Draw baseline-ish: top-align using negative bbox top so glyphs sit in cell.
        box = font.getbbox(chr(cp))
        ox = -box[0] if box else 0
        oy = -box[1] if box else 0
        d.text((ox, oy), chr(cp), fill=255, font=font)
        px_on = img.load()
        for gy in range(height):
            for byte_i in range(stride):
                b = 0
                for bit in range(8):
                    gx = byte_i * 8 + bit
                    on = gx < width and px_on[gx, gy] >= 128
                    if on:
                        b |= 1 << (7 - bit)
                rows.append(b)

    base = symbol
    header_guard = "_ui_fonts_%s_h" % symbol
    with open(out + ".h", "w") as f:
        f.write("// Generated by tools/mkfont.py from %s @ %dpx. Do not edit.\n" % (ttf, px))
        f.write("#ifndef %s\n#define %s\n#include \"../font.h\"\n" % (header_guard, header_guard))
        f.write("extern const Font %s;\n#endif\n" % base)
    with open(out + ".cpp", "w") as f:
        f.write("// Generated by tools/mkfont.py from %s @ %dpx. Do not edit.\n" % (ttf, px))
        f.write("#include \"%s.h\"\n" % out.split("/")[-1])
        f.write("static const uint8_t kBits[] = {\n")
        for i in range(0, len(rows), 12):
            f.write("  " + ",".join("0x%02X" % b for b in rows[i:i+12]) + ",\n")
        f.write("};\n")
        f.write("const Font %s = { 0x%02X, 0x%02X, %d, %d, %d, kBits };\n"
                % (base, FIRST, LAST, width, height, stride))
    print("wrote %s.h / %s.cpp  (%dx%d, stride %d)" % (out, out, width, height, stride))

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Generate the three fonts**

```bash
python3 -m pip install --quiet pillow
python3 tools/mkfont.py tools/fonts/PressStart2P-Regular.ttf 8  g_font_ps2p8    src/ui/fonts/font_ps2p8
python3 tools/mkfont.py tools/fonts/VT323-Regular.ttf       16 g_font_vt323_16 src/ui/fonts/font_vt323_16
python3 tools/mkfont.py tools/fonts/VT323-Regular.ttf       22 g_font_vt323_22 src/ui/fonts/font_vt323_22
```
Expected: three "wrote …" lines; six files appear under `src/ui/fonts/`.

- [ ] **Step 4: Write the failing test**

`test/test_fonts.cpp`:
```cpp
#include "../src/ui/fonts/font_ps2p8.h"
#include "../src/ui/fonts/font_vt323_16.h"
#include "../src/ui/fonts/font_vt323_22.h"
#include "../src/ui/glyph_draw.h"
#include <stdio.h>
#include <assert.h>

static bool glyph_nonblank(const Font *f, char c) {
    const uint8_t *g = f->bitmap +
        (unsigned)((unsigned char)c - f->first) * f->height * f->stride;
    for (unsigned i = 0; i < (unsigned) f->height * f->stride; i++)
        if (g[i]) return true;
    return false;
}

int main(void) {
    // Field sanity for the chosen sizes.
    assert(g_font_ps2p8.height == 8);
    assert(g_font_vt323_16.height == 16);
    assert(g_font_vt323_22.height == 22);
    assert(g_font_ps2p8.first == 0x20 && g_font_ps2p8.last == 0x7E);

    // Space is blank; letters are not.
    assert(!glyph_nonblank(&g_font_ps2p8, ' '));
    assert(glyph_nonblank(&g_font_ps2p8, 'A'));
    assert(glyph_nonblank(&g_font_vt323_16, 'A'));
    assert(glyph_nonblank(&g_font_vt323_22, 'g'));

    // Width math is consistent with the rasterizer.
    int w = gd_text_width(&g_font_ps2p8, 2, "AB");
    assert(w == 2 * g_font_ps2p8.width * 2);

    printf("test_fonts OK\n");
    return 0;
}
```

- [ ] **Step 5: Add the Makefile target and run to verify it builds + passes**

In `test/Makefile` add `test_fonts` to `run:` and `clean:`, with rule:
```make
test_fonts: test_fonts.cpp ../src/ui/fonts/font_ps2p8.cpp ../src/ui/fonts/font_vt323_16.cpp ../src/ui/fonts/font_vt323_22.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_fonts.cpp ../src/ui/fonts/font_ps2p8.cpp ../src/ui/fonts/font_vt323_16.cpp ../src/ui/fonts/font_vt323_22.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp
```
Run: `make -C test test_fonts && ./test/test_fonts`
Expected: `test_fonts OK`

- [ ] **Step 6: Commit**
```bash
git add tools/mkfont.py tools/fonts src/ui/fonts test/test_fonts.cpp test/Makefile
git commit -m "feat(ui): mkfont.py + baked Press Start 2P / VT323 bitmap fonts"
```

---

### Task 6: ROM browser layout math (`rom_layout`)

**Files:**
- Create: `src/menu/rom_layout.h`, `src/menu/rom_layout.cpp`
- Test: `test/test_rom_layout.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Produces:
  - `struct ScrollThumb { int y; int h; };`
  - `ScrollThumb scrollbar_thumb(int track_h, int count, int visible, int top);` — when `count <= visible` (or `count<=0`), thumb fills the track (`{0, track_h}`). Otherwise `h = max(track_h*visible/count, 8)`, `y` proportional to `top/(count-visible)`, clamped so `y+h <= track_h`.
  - `int rom_ext_offset(const char *name);` — index of the last `.` if there is ≥1 char before and ≥1 after it, else `-1`.

- [ ] **Step 1: Write the failing test**

`test/test_rom_layout.cpp`:
```cpp
#include "../src/menu/rom_layout.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    // everything fits -> full track
    ScrollThumb a = scrollbar_thumb(100, 5, 10, 0);
    assert(a.y == 0 && a.h == 100);

    // 20 items, 10 visible, at top -> half-height thumb at top
    ScrollThumb b = scrollbar_thumb(100, 20, 10, 0);
    assert(b.h == 50 && b.y == 0);

    // scrolled to the bottom -> thumb flush with the track bottom
    ScrollThumb c = scrollbar_thumb(100, 20, 10, 10);  // top can reach count-visible
    assert(c.y + c.h == 100);

    // thumb never smaller than 8px
    ScrollThumb d = scrollbar_thumb(100, 1000, 10, 0);
    assert(d.h >= 8);

    // extension offset
    assert(rom_ext_offset("Sonic.md") == 5);
    assert(rom_ext_offset("a.b.gen") == 3);   // last dot
    assert(rom_ext_offset("noext") == -1);
    assert(rom_ext_offset(".hidden") == -1);  // nothing before the dot
    assert(rom_ext_offset("trailing.") == -1);// nothing after the dot
    assert(rom_ext_offset("") == -1);

    printf("test_rom_layout OK\n");
    return 0;
}
```

- [ ] **Step 2: Add the Makefile target and run to verify it fails**

In `test/Makefile` add `test_rom_layout` to `run:` and `clean:`, with rule:
```make
test_rom_layout: test_rom_layout.cpp ../src/menu/rom_layout.cpp ../src/menu/rom_layout.h
	$(CXX) $(CXXFLAGS) -o $@ test_rom_layout.cpp ../src/menu/rom_layout.cpp
```
Run: `make -C test test_rom_layout`
Expected: FAIL — missing `rom_layout.h`.

- [ ] **Step 3: Write `rom_layout.h`**

`src/menu/rom_layout.h`:
```cpp
//
// src/menu/rom_layout.h
//
// Bare Metal Sega Genesis
// Pure layout math for the ROM browser (scrollbar geometry, extension split).
// No Circle deps (host-testable).
//
#ifndef _menu_rom_layout_h
#define _menu_rom_layout_h

struct ScrollThumb { int y; int h; };

// Thumb geometry within a vertical track of height track_h px, for a list of
// `count` items showing `visible` at once, scrolled so `top` is first visible.
ScrollThumb scrollbar_thumb(int track_h, int count, int visible, int top);

// Offset of the extension dot for display dimming: index of the last '.' if it
// has >=1 char before and >=1 char after, else -1.
int rom_ext_offset(const char *name);

#endif
```

- [ ] **Step 4: Write `rom_layout.cpp`**

`src/menu/rom_layout.cpp`:
```cpp
//
// src/menu/rom_layout.cpp
//
// Bare Metal Sega Genesis
// See rom_layout.h.
//
#include "rom_layout.h"
#include <string.h>

ScrollThumb scrollbar_thumb(int track_h, int count, int visible, int top) {
    ScrollThumb t;
    if (count <= 0 || visible <= 0 || count <= visible) {
        t.y = 0; t.h = track_h;
        return t;
    }
    int h = track_h * visible / count;
    if (h < 8) h = 8;
    if (h > track_h) h = track_h;

    int span = count - visible;            // max value of top
    if (top < 0) top = 0;
    if (top > span) top = span;
    int y = (track_h - h) * top / span;
    if (y < 0) y = 0;
    if (y + h > track_h) y = track_h - h;

    t.y = y; t.h = h;
    return t;
}

int rom_ext_offset(const char *name) {
    if (name == 0) return -1;
    int n = (int) strlen(name);
    for (int i = n - 1; i >= 0; i--) {
        if (name[i] == '.') {
            if (i > 0 && i < n - 1) return i;
            return -1;
        }
    }
    return -1;
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_rom_layout && ./test/test_rom_layout`
Expected: `test_rom_layout OK`

- [ ] **Step 6: Commit**
```bash
git add src/menu/rom_layout.h src/menu/rom_layout.cpp test/test_rom_layout.cpp test/Makefile
git commit -m "feat(menu): pure ROM browser layout math (rom_layout)"
```

---

### Task 7: Circle renderer wrapper (`GlyphCanvas`)

**Files:**
- Create: `src/ui/glyph_canvas.h`, `src/ui/glyph_canvas.cpp`
- Modify: root `Makefile` (add new OBJS)

No host test — this is thin delegation to already-tested pure functions; it is verified by the kernel cross-build in Task 9. (Per the test pyramid in the codebase: `overlay.cpp`/`splash_draw.cpp` Circle wrappers are likewise not host-tested.)

**Interfaces:**
- Consumes: `Display` (`Buffer()`, `Pitch()`, `Width()`, `Height()`); `gd_*` (Task 2); `icon_*` (Task 3); `Font` (Task 2).
- Produces (methods on `GlyphCanvas`):
  - `GlyphCanvas(Display *pDisplay);`
  - `unsigned Width() const; unsigned Height() const;`
  - `void Clear(u16 color);`
  - `void FillRect(int x,int y,int w,int h,u16 color);`
  - `void BlendRect(int x,int y,int w,int h,u16 color,u8 alpha);`
  - `void Scanlines(int x,int y,int w,int h,u8 strength);`
  - `int Text(const Font *f,int scale,int x,int y,const char *s,u16 fg,u16 bg,bool transparent);`
  - `int TextWidth(const Font *f,int scale,const char *s) const;`
  - `void IconPlay(int x,int y,int size,u16 color);`
  - `void IconCross(int x,int y,int size,u16 color);`
  - `void IconButton(int x,int y,int d,char letter,u16 fill,u16 fg,const Font *f);`

- [ ] **Step 1: Write `glyph_canvas.h`**

`src/ui/glyph_canvas.h`:
```cpp
//
// src/ui/glyph_canvas.h
//
// Bare Metal Sega Genesis
// Circle binding for the pure glyph_draw/icons primitives: draws into the
// Display's RGB565 front page. Successor to TextCanvas for redesigned screens.
//
#ifndef _ui_glyph_canvas_h
#define _ui_glyph_canvas_h
#include <circle/types.h>
#include "font.h"
#include "../video/display.h"

class GlyphCanvas
{
public:
    GlyphCanvas(Display *pDisplay);

    unsigned Width(void) const;
    unsigned Height(void) const;

    void Clear(u16 color);
    void FillRect(int x, int y, int w, int h, u16 color);
    void BlendRect(int x, int y, int w, int h, u16 color, u8 alpha);
    void Scanlines(int x, int y, int w, int h, u8 strength);

    int  Text(const Font *f, int scale, int x, int y, const char *s,
              u16 fg, u16 bg, bool transparent);
    int  TextWidth(const Font *f, int scale, const char *s) const;

    void IconPlay (int x, int y, int size, u16 color);
    void IconCross(int x, int y, int size, u16 color);
    void IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                    const Font *f);

private:
    Display *m_pDisplay;
};

#endif
```

- [ ] **Step 2: Write `glyph_canvas.cpp`**

`src/ui/glyph_canvas.cpp`:
```cpp
//
// src/ui/glyph_canvas.cpp
//
// Bare Metal Sega Genesis
// See glyph_canvas.h.
//
#include "glyph_canvas.h"
#include "glyph_draw.h"
#include "icons.h"

GlyphCanvas::GlyphCanvas(Display *pDisplay) : m_pDisplay(pDisplay) {}

unsigned GlyphCanvas::Width(void)  const { return m_pDisplay->Width(); }
unsigned GlyphCanvas::Height(void) const { return m_pDisplay->Height(); }

void GlyphCanvas::FillRect(int x, int y, int w, int h, u16 color) {
    gd_fill_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x, y, w, h, color);
}

void GlyphCanvas::Clear(u16 color) {
    FillRect(0, 0, (int) m_pDisplay->Width(), (int) m_pDisplay->Height(), color);
}

void GlyphCanvas::BlendRect(int x, int y, int w, int h, u16 color, u8 alpha) {
    gd_blend_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                  (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                  x, y, w, h, color, alpha);
}

void GlyphCanvas::Scanlines(int x, int y, int w, int h, u8 strength) {
    gd_scanlines(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x, y, w, h, strength);
}

int GlyphCanvas::Text(const Font *f, int scale, int x, int y, const char *s,
                      u16 fg, u16 bg, bool transparent) {
    return gd_draw_text(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                        (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                        f, scale, x, y, s, fg, bg, transparent);
}

int GlyphCanvas::TextWidth(const Font *f, int scale, const char *s) const {
    return gd_text_width(f, scale, s);
}

void GlyphCanvas::IconPlay(int x, int y, int size, u16 color) {
    icon_play(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
              (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
              x, y, size, color);
}

void GlyphCanvas::IconCross(int x, int y, int size, u16 color) {
    icon_cross(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
               (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
               x, y, size, color);
}

void GlyphCanvas::IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                             const Font *f) {
    icon_button(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                x, y, d, letter, fill, fg, f);
}
```

- [ ] **Step 3: Register new objects in the kernel build**

In root `Makefile`, in the `OBJS = …` list, add these lines next to `src/ui/text_canvas.o`:
```
       src/ui/pixel_ops.o \
       src/ui/glyph_draw.o \
       src/ui/icons.o \
       src/ui/glyph_canvas.o \
       src/ui/fonts/font_ps2p8.o \
       src/ui/fonts/font_vt323_16.o \
       src/ui/fonts/font_vt323_22.o \
```

- [ ] **Step 4: Commit**
```bash
git add src/ui/glyph_canvas.h src/ui/glyph_canvas.cpp Makefile
git commit -m "feat(ui): GlyphCanvas Circle wrapper + register UI objects in kernel build"
```

---

### Task 8: Re-skin the ROM browser (pilot)

**Files:**
- Modify: `src/menu/rom_menu.h` (canvas type → `GlyphCanvas`, add include)
- Modify: `src/menu/rom_menu.cpp` (full re-skin of `Render` + empty state)
- Modify: `src/kernel.h`, `src/kernel.cpp` (construct `GlyphCanvas`, pass to `RomMenu`)

No host test (render-layer/Circle). Verified by kernel build (Task 9) + hardware. Control flow (`Run`, `Scan`, button handling) is unchanged — only header/include types and the `Render`/empty-state drawing change.

**Interfaces:**
- Consumes: `GlyphCanvas` (Task 7), `theme` (Task 4), `g_font_ps2p8`/`g_font_vt323_22` (Task 5), `scrollbar_thumb`/`rom_ext_offset` (Task 6), existing `MenuState`/`menu_move`/`Entry`.

- [ ] **Step 1: Update `rom_menu.h`**

Replace the `text_canvas.h` include and the `TextCanvas` references:
```cpp
#include "../ui/glyph_canvas.h"
```
(remove `#include "../ui/text_canvas.h"`)

Change the constructor signature and member type:
```cpp
    RomMenu(GlyphCanvas *pCanvas, Gamepad *pGamepad,
            Storage *pStorage, CUSBHCIDevice *pUSBHCI);
```
```cpp
    GlyphCanvas   *m_pCanvas;
```

- [ ] **Step 2: Rewrite `rom_menu.cpp` head + `Render` + empty state**

Replace the color constants block and the constructor signature, then replace `Render` and the empty-state block inside `Run`. New top-of-file includes/constants:
```cpp
#include "rom_menu.h"
#include "menu_path.h"
#include "rom_layout.h"
#include "../ui/theme.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
#include "../input/joypad_map.h"   // GP_UP, GP_DOWN, GP_START
#include <circle/timer.h>
#include <circle/util.h>           // strcpy, strcmp, strlen

static const char ROOT[] = "SD:/roms";

// Layout constants (pixels), tuned for 1080p; safe on smaller modes via clipping.
static const int PAD       = 40;   // left/right margin
static const int TOP       = 36;   // header baseline area
static const int ROW_H     = 30;   // body row height (VT323 22 + spacing)
static const int LIST_TOP  = 96;   // first list row y
static const int FOOT_H    = 40;   // footer band height
```

Constructor (signature change only):
```cpp
RomMenu::RomMenu(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                 Storage *pStorage, CUSBHCIDevice *pUSBHCI)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad),
    m_pStorage(pStorage), m_pUSBHCI(pUSBHCI), m_count(0)
{
    m_path[0] = '\0';
}
```

New `Render`:
```cpp
void RomMenu::Render(const MenuState &s)
{
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();
    int listBottom = H - FOOT_H - 10;

    m_pCanvas->Clear(theme::BG);

    // Header: title (Press Start 2P x2) + path (cyan) + green P1 dot.
    m_pCanvas->Text(&g_font_ps2p8, 2, PAD, TOP, "ROM BROWSER",
                    theme::WHITE, theme::BG, true);
    int pw = m_pCanvas->TextWidth(&g_font_vt323_22, 1, m_path);
    int dotx = W - PAD - 14;
    m_pCanvas->Text(&g_font_vt323_22, 1, dotx - pw - 18, TOP, m_path,
                    theme::VALUE, theme::BG, true);
    m_pCanvas->FillRect(dotx, TOP + 4, 10, 10, theme::ACTIVE);
    m_pCanvas->Text(&g_font_vt323_22, 1, dotx - pw - 18 + pw + 22, TOP, "P1",
                    theme::ACTIVE, theme::BG, true);
    m_pCanvas->FillRect(PAD, TOP + 34, W - 2 * PAD, 2, theme::TEXT_DIM);

    // List rows.
    int rowW = W - 2 * PAD - 14;       // leave room for the scrollbar
    for (int i = 0; i < s.visible_rows; i++)
    {
        int idx = s.top + i;
        if (idx >= s.count) break;
        int ty = LIST_TOP + i * ROW_H;
        if (ty + ROW_H > listBottom) break;

        bool sel = (idx == s.selected);
        const Entry &e = m_entries[idx];

        if (sel)
            m_pCanvas->FillRect(PAD - 6, ty - 2, rowW + 12, ROW_H, theme::SELECTION);

        int tx = PAD + 22;
        if (sel)
            m_pCanvas->IconPlay(PAD - 2, ty + 4, 16, theme::WHITE);

        if (e.is_dir)
        {
            char buf[270];
            buf[0] = '['; buf[1] = ' ';
            unsigned n = 0; while (e.name[n] && n < 260) { buf[2 + n] = e.name[n]; n++; }
            buf[2 + n] = ' '; buf[3 + n] = ']'; buf[4 + n] = '\0';
            m_pCanvas->Text(&g_font_vt323_22, 1, tx, ty, buf,
                            sel ? theme::WHITE : theme::VALUE, theme::BG, true);
        }
        else
        {
            int ext = rom_ext_offset(e.name);
            if (ext < 0)
            {
                m_pCanvas->Text(&g_font_vt323_22, 1, tx, ty, e.name,
                                sel ? theme::WHITE : theme::TEXT, theme::BG, true);
            }
            else
            {
                char stem[270];
                int k = 0; while (k < ext && k < 268) { stem[k] = e.name[k]; k++; }
                stem[k] = '\0';
                int ex = m_pCanvas->Text(&g_font_vt323_22, 1, tx, ty, stem,
                                         sel ? theme::WHITE : theme::TEXT, theme::BG, true);
                m_pCanvas->Text(&g_font_vt323_22, 1, ex, ty, e.name + ext,
                                sel ? theme::WHITE : theme::TEXT_DIM, theme::BG, true);
            }
        }
    }

    // Scrollbar.
    int trackX = W - PAD - 6;
    int trackY = LIST_TOP;
    int trackH = (s.visible_rows) * ROW_H;
    m_pCanvas->FillRect(trackX, trackY, 6, trackH, theme::TEXT_DIM);
    ScrollThumb th = scrollbar_thumb(trackH, s.count, s.visible_rows, s.top);
    m_pCanvas->FillRect(trackX, trackY + th.y, 6, th.h, theme::SELECTION);

    // Footer hint bar.
    int fy = H - FOOT_H + 8;
    m_pCanvas->FillRect(PAD, H - FOOT_H - 2, W - 2 * PAD, 2, theme::TEXT_DIM);
    m_pCanvas->IconCross(PAD, fy, 16, theme::TEXT);
    m_pCanvas->Text(&g_font_ps2p8, 1, PAD + 24, fy + 4, "NAVIGATE",
                    theme::TEXT_DIM, theme::BG, true);
    int bx = PAD + 24 + m_pCanvas->TextWidth(&g_font_ps2p8, 1, "NAVIGATE") + 30;
    m_pCanvas->IconButton(bx, fy, 16, 'A', theme::SELECTION, theme::WHITE, &g_font_ps2p8);
    m_pCanvas->Text(&g_font_ps2p8, 1, bx + 24, fy + 4, "LAUNCH",
                    theme::TEXT_DIM, theme::BG, true);

    // Count, right-aligned.
    int dirs = 0, files = 0;
    for (int i = 0; i < m_count; i++) {
        if (m_entries[i].is_dir) { if (strcmp(m_entries[i].name, "..") != 0) dirs++; }
        else files++;
    }
    char count[48];
    // build "N ROMS  M FOLDERS"
    int p = 0;
    p += __builtin_snprintf(count + p, sizeof(count) - p, "%d ROMS  %d FOLDERS", files, dirs);
    (void) p;
    int cw = m_pCanvas->TextWidth(&g_font_ps2p8, 1, count);
    m_pCanvas->Text(&g_font_ps2p8, 1, W - PAD - cw, fy + 4, count,
                    theme::TEXT_DIM, theme::BG, true);

    // CRT scanlines over everything.
    m_pCanvas->Scanlines(0, 0, W, H, 60);
}
```

> Note: `__builtin_snprintf` avoids needing extra includes in the freestanding build; it is available with the GCC toolchain used here. If it fails to link, replace with manual integer-to-string formatting.

New empty-state block (replace the `if (m_count == 0) { … }` body inside `Run`):
```cpp
    if (m_count == 0)
    {
        int W = (int) m_pCanvas->Width();
        int H = (int) m_pCanvas->Height();
        m_pCanvas->Clear(theme::BG);
        m_pCanvas->Text(&g_font_ps2p8, 2, PAD, TOP, "ROM BROWSER",
                        theme::WHITE, theme::BG, true);
        m_pCanvas->FillRect(PAD, TOP + 34, W - 2 * PAD, 2, theme::TEXT_DIM);

        // Cartridge glyph (simple block) centered.
        int cx = W / 2 - 31, cy = H / 2 - 80;
        m_pCanvas->FillRect(cx, cy, 62, 78, theme::TEXT_DIM);
        m_pCanvas->FillRect(cx + 6, cy + 6, 50, 50, theme::BG);

        const char *t1 = "NO ROMS FOUND";
        int t1w = m_pCanvas->TextWidth(&g_font_ps2p8, 2, t1);
        m_pCanvas->Text(&g_font_ps2p8, 2, W / 2 - t1w / 2, H / 2 + 20,
                        t1, theme::WHITE, theme::BG, true);
        const char *t2 = "Place .md / .bin / .gen files in SD:/roms then reboot.";
        int t2w = m_pCanvas->TextWidth(&g_font_vt323_22, 1, t2);
        m_pCanvas->Text(&g_font_vt323_22, 1, W / 2 - t2w / 2, H / 2 + 56,
                        t2, theme::TEXT_MUTED, theme::BG, true);

        m_pCanvas->FillRect(PAD, H - FOOT_H + 6, 10, 10, theme::ADJUST);
        m_pCanvas->Text(&g_font_ps2p8, 1, PAD + 18, H - FOOT_H + 8,
                        "WAITING FOR MEDIA", theme::ADJUST, theme::BG, true);
        m_pCanvas->Scanlines(0, 0, W, H, 60);
        return false;
    }
```

Also update the `visible` calculation inside `Run` to pixel rows:
```cpp
    int visible = ((int) m_pCanvas->Height() - LIST_TOP - FOOT_H - 12) / ROW_H;
    if (visible < 1) visible = 1;
```

- [ ] **Step 3: Wire `GlyphCanvas` into the kernel**

In `src/kernel.h`, next to `TextCanvas m_Canvas;` add:
```cpp
	GlyphCanvas        m_GlyphCanvas;  // redesigned-screen renderer (pilot: ROM browser)
```
Add the include near the `text_canvas.h` include:
```cpp
#include "ui/glyph_canvas.h"
```

In `src/kernel.cpp` constructor initializer list: add `m_GlyphCanvas (&m_Display),` (after `m_Canvas (&m_Display),`) and change the `m_RomMenu` line to use it:
```cpp
	m_RomMenu (&m_GlyphCanvas, &m_Gamepad, &m_Storage, &m_USBHCI),
```
(Member initialization order: `m_GlyphCanvas` is declared after `m_Display`, so this is safe.)

- [ ] **Step 4: Commit**
```bash
git add src/menu/rom_menu.h src/menu/rom_menu.cpp src/kernel.h src/kernel.cpp
git commit -m "feat(menu): re-skin ROM browser to the v1.0.0 design (GlyphCanvas pilot)"
```

---

### Task 9: Full host-test sweep + kernel build + hardware-verify entry

**Files:**
- Modify: a project checklist/docs file recording the new hardware-verify item (match the project's existing verify-log convention).

- [ ] **Step 1: Run the entire host test suite**

Run: `make -C test run`
Expected: every line ends in `OK`, including `test_pixel_ops`, `test_glyph_draw`, `test_icons`, `test_theme`, `test_fonts`, `test_rom_layout`.

- [ ] **Step 2: Cross-build the kernel**

Run: `make`
Expected: builds `kernel7.img` with no errors. If `__builtin_snprintf` fails to link, apply the fallback noted in Task 8 and rebuild.

- [ ] **Step 3: Record the hardware-verify item**

Add a new checklist entry (matching the project's existing verify-log format and IDs — see the most recent "hardware-verify pass" commit for the convention) covering: ROM browser renders with the new look (header, list with folder/file rows + dim extensions, selection highlight + play marker, scrollbar, footer hint bar + counts, scanlines) and the empty-state screen. Mark it pending hardware verification.

- [ ] **Step 4: Commit**
```bash
git add -A
git commit -m "docs: add hardware-verify item for ROM browser redesign pilot"
```

- [ ] **Step 5: Flash and verify on hardware (manual)**

Copy `kernel7.img` to the SD card, boot the Pi 2, and confirm the ROM browser and empty state match the design intent on the TV. This is the pilot acceptance gate before rolling the pattern out to the other 8 screens.

---

## Self-Review

**Spec coverage:**
- Font pipeline (mkfont + baked fonts, PS2P 8px + VT323 16/22) → Task 5. ✓
- `GlyphCanvas` with FillRect/BlendRect/DrawText(transparent)/Scanlines → Tasks 2 (pure), 7 (wrapper). ✓
- Theme module (palette + roles) → Task 4. ✓
- ROM-browser pilot (header, list, folder/file + dim ext, selection+marker, scrollbar, footer hints, counts, empty state, scanlines) → Tasks 6 (math) + 8 (render). ✓
- Testing (mkfont output, RGB565/blend math, glyph blit into buffer, layout helpers) → Tasks 1,2,5,6. ✓ Hardware verify → Task 9. ✓
- Icon helpers (▶ ✛ Ⓐ) → Task 3. ✓
- Out-of-scope items (gradients/glow/runtime picker) → not built. ✓
- Rollout to other 8 screens → intentionally deferred (post-pilot), per the spec's pilot-then-rollout decision.

**Placeholder scan:** No TBD/TODO; every code step contains full code. The one runtime caveat (`__builtin_snprintf`) has an explicit fallback. ✓

**Type consistency:** `Font` fields (`first,last,width,height,stride,bitmap`) are used identically in Tasks 2, 3, 5, 8. `gd_*`/`icon_*` signatures match between definition (Tasks 2/3) and the `GlyphCanvas` calls (Task 7). `ScrollThumb{y,h}` and `scrollbar_thumb`/`rom_ext_offset` match between Task 6 and Task 8. `GlyphCanvas` method set defined in Task 7 matches the calls in Task 8. ✓
