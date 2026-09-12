# UI Scaling by Output Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every GlyphCanvas-drawn screen (menus, splash, HUD) grow in whole-number steps with the HDMI output resolution so the UI stays readable at 1080p and above.

**Architecture:** A pure `ui_scale(w, h)` picks an integer scale. `GlyphCanvas` reports a logical size (physical ÷ scale) and multiplies every coordinate, size and font scale on the way to the framebuffer, so no screen code changes. Three pure primitives (`gd_scanlines`, `gd_stipple_rect`, `icon_button`) gain a `scale` parameter (default 1 = pixel-identical to today); the HUD's own `hud_scale` becomes redundant and is removed.

**Tech Stack:** C++ bare metal (Circle, `arm-linux-gnueabihf-` cross toolchain, Raspberry Pi 2 default); host unit tests with the system compiler (`test/Makefile`, `assert`-based).

**Spec:** `docs/superpowers/specs/2026-09-11-ui-scaling-design.md`

## Global Constraints

- Scale rule, exactly: `ui_scale(fb_w, fb_h) = max(1, min(fb_h / 480, fb_w / 640))`, integer division, never 0.
- Every primitive change takes a trailing `int scale` parameter **defaulting to 1**; scale 1 output must be pixel-identical to the current code.
- `gd_stipple_rect` must stay write-only (never read the framebuffer) — HUD performance constraint.
- Pure modules (`src/ui/ui_scale.*`, `glyph_draw.*`, `icons.*`, `hud.*`) have no Circle includes and are host-tested (`cd test && make run`).
- No changes to screen code (`src/menu/*`, `src/ui/screen_chrome.*`, `src/video/splash_draw.cpp`), `Display::Blit`, or `TextCanvas`.
- Bare metal: no `snprintf`; match surrounding style (4-space indent in `src/ui/*`).
- Build target: Pi 2 (`make -j$(nproc)`, `RASPPI=2` default). Before handing any image to the user, every `libs/**/*.a`, `src/**/*.o` and `kernel7.elf` must report `Tag_CPU_arch: v7` (`arm-linux-gnueabihf-readelf -A`); otherwise `make clean-all` and rebuild.
- All commands assume the repo root (`/home/matt/Bare-Metal-Sega-Genesis`). Commands written `cd test && …` must be run in a subshell — `(cd test && …)` — so the next step (git, `make` for the kernel) still runs from the repo root.
- Work on branch `feat/ui-scaling`. Every commit message ends with:
  ```
  Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj
  ```

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `src/ui/ui_scale.h` / `.cpp` | Create | Pure integer UI-scale rule |
| `test/test_ui_scale.cpp` | Create | Host test for the rule |
| `src/ui/glyph_draw.h` / `.cpp` | Modify | `scale` param for `gd_scanlines`, `gd_stipple_rect` |
| `test/test_glyph_draw.cpp` | Modify | Scale-2 scanline/stipple tests |
| `src/ui/icons.h` / `.cpp` | Modify | `scale` param for `icon_button` letter |
| `test/test_icons.cpp` | Modify | Exact scale-1 + scale-2 button tests |
| `src/ui/glyph_canvas.h` / `.cpp` | Modify | Logical size + logical→physical mapping |
| `src/ui/hud.h` / `.cpp`, `test/test_hud.cpp` | Modify | Remove `hud_scale` |
| `src/ui/overlay.cpp` | Modify | Drop `* sc` (canvas scales) |
| `Makefile`, `test/Makefile` | Modify | Build `ui_scale.o` / `test_ui_scale` |
| `docs/hardware-checklist-ui-scaling.md` | Create | Hardware checklist Y |

---

### Task 1: Pure `ui_scale` rule

**Files:**
- Create: `src/ui/ui_scale.h`, `src/ui/ui_scale.cpp`
- Create: `test/test_ui_scale.cpp`
- Modify: `test/Makefile` (run list line 9, run commands after line 36, new rule after line 117, clean line 120)

**Interfaces:**
- Consumes: nothing.
- Produces: `unsigned ui_scale(unsigned fb_w, unsigned fb_h);` and constants `UI_SCALE_MIN_W` (640), `UI_SCALE_MIN_H` (480), declared in `src/ui/ui_scale.h`. Used by Task 4.

- [ ] **Step 1: Write the failing test**

Create `test/test_ui_scale.cpp`:

```cpp
#include "../src/ui/ui_scale.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // Standard HDMI modes: whole-number steps, logical space >= 640x480.
    assert(ui_scale(720,  480)  == 1);   // 480p
    assert(ui_scale(1280, 720)  == 1);   // 720p
    assert(ui_scale(1920, 1080) == 2);   // 1080p -> 960x540
    assert(ui_scale(2560, 1440) == 3);   // 1440p -> 853x480
    assert(ui_scale(3840, 2160) == 4);   // 4K    -> 960x540

    // Non-16:9 native modes.
    assert(ui_scale(1280, 1024) == 2);   // -> 640x512
    assert(ui_scale(1024, 768)  == 1);
    assert(ui_scale(1024, 1024) == 1);   // width-limited: height alone gives 2

    // Degenerate / tiny sizes never return 0.
    assert(ui_scale(0, 0)       == 1);
    assert(ui_scale(320, 240)   == 1);
    assert(ui_scale(1920, 0)    == 1);

    printf("test_ui_scale: OK\n");
    return 0;
}
```

In `test/Makefile`:
- Append ` test_ui_scale` to the end of the `run:` prerequisite line (line 9, currently ending `test_poll_gate test_pad_test`).
- After the line `	./test_pad_test` add the line `	./test_ui_scale` (tab-indented).
- After the `test_pad_test:` rule (its recipe line ends `../src/menu/pad_test.cpp`) add:

```make

test_ui_scale: test_ui_scale.cpp ../src/ui/ui_scale.cpp ../src/ui/ui_scale.h
	$(CXX) $(CXXFLAGS) -o $@ test_ui_scale.cpp ../src/ui/ui_scale.cpp
```

- Append ` test_ui_scale` to the end of the `clean:` `rm -f` line.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test && make test_ui_scale`
Expected: FAIL — `make: *** No rule to make target '../src/ui/ui_scale.cpp', needed by 'test_ui_scale'.  Stop.`

- [ ] **Step 3: Write minimal implementation**

Create `src/ui/ui_scale.h`:

```cpp
//
// src/ui/ui_scale.h
//
// Bare Metal Sega Genesis
// Integer UI scale for the current output resolution. The framebuffer IS the
// HDMI mode, so fixed-pixel menus shrink as the mode grows; GlyphCanvas divides
// the physical size by this scale to get a logical drawing surface that is
// always at least UI_SCALE_MIN_W x UI_SCALE_MIN_H (every screen fits 640x480).
// Whole-number steps keep the bitmap fonts pixel-perfect. Pure; no Circle deps.
//

#ifndef _ui_ui_scale_h
#define _ui_ui_scale_h

#define UI_SCALE_MIN_W 640
#define UI_SCALE_MIN_H 480

// max(1, min(fb_h / UI_SCALE_MIN_H, fb_w / UI_SCALE_MIN_W)).
// 480p/720p -> 1, 1080p -> 2, 1440p -> 3. Never returns 0.
unsigned ui_scale(unsigned fb_w, unsigned fb_h);

#endif
```

Create `src/ui/ui_scale.cpp`:

```cpp
//
// src/ui/ui_scale.cpp
//
// Bare Metal Sega Genesis
// See ui_scale.h.
//

#include "ui_scale.h"

unsigned ui_scale(unsigned fb_w, unsigned fb_h)
{
    unsigned sh = fb_h / UI_SCALE_MIN_H;
    unsigned sw = fb_w / UI_SCALE_MIN_W;
    unsigned s  = sh < sw ? sh : sw;
    return s < 1 ? 1 : s;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd test && make test_ui_scale && ./test_ui_scale`
Expected: `test_ui_scale: OK`, no compiler warnings.

- [ ] **Step 5: Commit**

```bash
git add src/ui/ui_scale.h src/ui/ui_scale.cpp test/test_ui_scale.cpp test/Makefile
git commit -F - <<'EOF'
feat(ui): pure ui_scale rule for resolution-based UI scaling

max(1, min(h/480, w/640)): 480p/720p -> 1, 1080p -> 2, 1440p -> 3; the
logical surface never drops below 640x480. Host-tested incl. non-16:9
native modes and degenerate sizes.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj
EOF
```

---

### Task 2: Scale-aware scanlines and stipple

**Files:**
- Modify: `src/ui/glyph_draw.h` (declarations of `gd_scanlines`, `gd_stipple_rect`)
- Modify: `src/ui/glyph_draw.cpp` (`gd_scanlines`, `gd_stipple_rect` bodies)
- Test: `test/test_glyph_draw.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `void gd_scanlines(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int w, int h, uint8_t strength, int scale = 1);`
  - `void gd_stipple_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int w, int h, uint16_t color, int scale = 1);`
  Used by Task 4 (`GlyphCanvas::Scanlines`, `GlyphCanvas::StippleRect`).

- [ ] **Step 1: Write the failing tests**

In `test/test_glyph_draw.cpp`, insert immediately before the line `    // text width = chars * width * scale`:

```cpp
    // scanlines at scale 2: 2-px dark bands on rows where (y / 2) % 3 == 2,
    // aligned to ABSOLUTE framebuffer rows (so region passes line up).
    reset();
    gd_fill_rect(fb, W, W, H, 0, 0, 8, 12, 0xFFFF);
    gd_scanlines(fb, W, W, H, 0, 0, 8, 12, 128, 2);
    assert(fb[3 * W + 0]  == 0xFFFF);         // row 3: (3/2)%3 = 1
    assert(fb[4 * W + 0]  != 0xFFFF);         // rows 4-5: band
    assert(fb[5 * W + 0]  != 0xFFFF);
    assert(fb[6 * W + 0]  == 0xFFFF);         // rows 6-9 untouched
    assert(fb[9 * W + 0]  == 0xFFFF);
    assert(fb[10 * W + 0] != 0xFFFF);         // rows 10-11: next band
    assert(fb[11 * W + 0] != 0xFFFF);

    // scale-2 region starting mid-band stays on the absolute row grid.
    reset();
    gd_fill_rect(fb, W, W, H, 0, 0, 8, 16, 0xFFFF);
    gd_scanlines(fb, W, W, H, 0, 5, 8, 6, 128, 2);
    assert(fb[4 * W + 0]  == 0xFFFF);         // outside region (would be band)
    assert(fb[5 * W + 0]  != 0xFFFF);         // (5/2)%3 == 2
    assert(fb[6 * W + 0]  == 0xFFFF);         // (6/2)%3 == 0
    assert(fb[10 * W + 0] != 0xFFFF);         // (10/2)%3 == 2

    // stipple at scale 2: 2x2 checker cells ((x/2 + y/2) & 1), gap rows where
    // ((y - y0) / 2) % 3 == 2. Still write-only.
    reset();
    gd_stipple_rect(fb, W, W, H, 0, 0, 8, 12, 0xABCD, 2);
    assert(fb[0 * W + 0] == 0xABCD);          // cell (0,0)
    assert(fb[0 * W + 1] == 0xABCD);          // same cell
    assert(fb[1 * W + 1] == 0xABCD);          // same cell
    assert(fb[0 * W + 2] == 0x0000);          // cell (1,0): odd
    assert(fb[2 * W + 2] == 0xABCD);          // cell (1,1): even
    assert(fb[4 * W + 0] == 0x0000);          // local rows 4-5: gap
    assert(fb[5 * W + 1] == 0x0000);
    assert(fb[6 * W + 2] == 0xABCD);          // row 6 resumes: cell (1,3) even
    assert(fb[6 * W + 0] == 0x0000);          // cell (0,3): odd

    // stipple scale-2 gap rows are LOCAL to the rect's y origin.
    reset();
    gd_stipple_rect(fb, W, W, H, 0, 3, 4, 8, 0x5555, 2);
    assert(fb[6 * W + 2] == 0x5555);          // local (6-3)/2 = 1; cell (1,3) even
    assert(fb[7 * W + 2] == 0x0000);          // local (7-3)/2 = 2 -> gap
    assert(fb[8 * W + 0] == 0x0000);          // local (8-3)/2 = 2 -> gap
    assert(fb[9 * W + 0] == 0x5555);          // local 3; cell (0,4) even

```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test && make test_glyph_draw`
Expected: FAIL — compile error `too many arguments to function 'void gd_scanlines(...)'` (and the same for `gd_stipple_rect`).

- [ ] **Step 3: Write minimal implementation**

In `src/ui/glyph_draw.h`, replace the `gd_scanlines` declaration and the stipple comment + declaration with:

```cpp
// Darken rows where (y / scale) % 3 == 2 (absolute framebuffer rows, so
// separate region passes stay aligned). scale 1 = every 3rd row.
void gd_scanlines (uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                   int x, int y, int w, int h, uint8_t strength, int scale = 1);

// Write-only pseudo-transparent fill: writes `color` on a 50% checkerboard of
// scale x scale cells and leaves rows where ((y - y0) / scale) % 3 == 2
// untouched as a scanline gap. Never READS the framebuffer (unlike
// blend_rect/scanlines), so it is cheap over the Pi's write-combining
// framebuffer where reads stall. Used for HUD/toast panels.
void gd_stipple_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                     int x, int y, int w, int h, uint16_t color, int scale = 1);
```

In `src/ui/glyph_draw.cpp`, replace the whole `gd_scanlines` and `gd_stipple_rect` functions with:

```cpp
void gd_scanlines(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                  int x, int y, int w, int h, uint8_t strength, int scale) {
    if (scale < 1) scale = 1;
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= fbh) continue;
        if (((yy / scale) % 3) != 2) continue;  // darken every 3rd (scaled) row
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || xx >= fbw) continue;
            uint16_t *p = &buf[(unsigned) yy * pitchPx + (unsigned) xx];
            *p = rgb565_blend(*p, 0x0000, strength);
        }
    }
}

void gd_stipple_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                     int x, int y, int w, int h, uint16_t color, int scale) {
    if (scale < 1) scale = 1;
    for (int yy = y; yy < y + h; yy++) {
        if (((yy - y) / scale) % 3 == 2) continue;          // scanline gap row
        for (int xx = x; xx < x + w; xx++) {
            if (((xx / scale + yy / scale) & 1) != 0) continue;  // 50% checker
            put(buf, pitchPx, fbw, fbh, xx, yy, color);
        }
    }
}
```

(At scale 1 these reduce to the previous `yy % 3`, `(yy - y) % 3` and `(xx + yy) & 1` tests, so existing output is unchanged.)

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd test && make test_glyph_draw test_icons test_fonts && ./test_glyph_draw && ./test_icons && ./test_fonts`
Expected: `test_glyph_draw OK`, `test_icons OK`, `test_fonts OK` (the old scale-1 scanline/stipple asserts still pass), no warnings.

- [ ] **Step 5: Commit**

```bash
git add src/ui/glyph_draw.h src/ui/glyph_draw.cpp test/test_glyph_draw.cpp
git commit -F - <<'EOF'
feat(ui): scale parameter for gd_scanlines and gd_stipple_rect

Scanline bands become scale px tall on the absolute row grid; stipple
uses scale x scale checker cells with local gap rows, still write-only.
Default scale 1 is pixel-identical to the previous output.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj
EOF
```

---

### Task 3: Scale-aware `icon_button` letter

**Files:**
- Modify: `src/ui/icons.h` (`icon_button` declaration)
- Modify: `src/ui/icons.cpp` (`icon_button` body)
- Test: `test/test_icons.cpp`

**Interfaces:**
- Consumes: `gd_text_width`, `gd_draw_text`, `gd_fill_rect` (unchanged, `src/ui/glyph_draw.h`).
- Produces: `void icon_button(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int d, char letter, uint16_t fill, uint16_t fg, const Font *f, int scale = 1);` Used by Task 4 (`GlyphCanvas::IconButton`).

- [ ] **Step 1: Write the failing tests**

In `test/test_icons.cpp`, replace the block that starts `    // button: fill present, and at least one fg (letter) pixel inside` and ends with `    assert(sawLetter);` with:

```cpp
    // button: fill present, and at least one fg (letter) pixel inside
    reset();
    icon_button(fb, W, W, H, 0, 0, 11, '!', 0xE000, 0xFFFF, &kFont);
    assert(fb[0 * W + 0] == 0xE000);          // fill corner
    bool sawLetter = false;
    for (unsigned i = 0; i < W * H; i++) if (fb[i] == 0xFFFF) sawLetter = true;
    assert(sawLetter);
    // exact scale-1 placement: 3x5 glyph centred in 11 -> origin (4,3); the
    // '!' stroke is the glyph's middle column -> x = 5, rows 3..7.
    assert(fb[3 * W + 5] == 0xFFFF);
    assert(fb[7 * W + 5] == 0xFFFF);
    assert(fb[3 * W + 6] == 0xE000);
    assert(fb[8 * W + 5] == 0xE000);

    // button at scale 2: letter drawn 2x (6x10) and centred in d=12 ->
    // origin (3,1); stroke columns x = 5..6, rows 1..10.
    reset();
    icon_button(fb, W, W, H, 0, 0, 12, '!', 0xE000, 0xFFFF, &kFont, 2);
    assert(fb[1 * W + 5]  == 0xFFFF);
    assert(fb[1 * W + 6]  == 0xFFFF);
    assert(fb[10 * W + 6] == 0xFFFF);
    assert(fb[1 * W + 4]  == 0xE000);         // left of stroke
    assert(fb[1 * W + 7]  == 0xE000);         // right of stroke
    assert(fb[0 * W + 5]  == 0xE000);         // above letter
    assert(fb[11 * W + 5] == 0xE000);         // below letter
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test && make test_icons`
Expected: FAIL — compile error `too many arguments to function 'void icon_button(...)'`.

- [ ] **Step 3: Write minimal implementation**

In `src/ui/icons.h`, replace the `icon_button` declaration with:

```cpp
// Filled d x d square with `letter` centred in it, drawn at font `scale`.
void icon_button(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                 int x, int y, int d, char letter,
                 uint16_t fill, uint16_t fg, const Font *f, int scale = 1);
```

In `src/ui/icons.cpp`, replace the whole `icon_button` function with:

```cpp
void icon_button(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                 int x, int y, int d, char letter,
                 uint16_t fill, uint16_t fg, const Font *f, int scale) {
    if (scale < 1) scale = 1;
    gd_fill_rect(buf, pitchPx, fbw, fbh, x, y, d, d, fill);
    if (f) {
        char s[2] = { letter, '\0' };
        int tw = gd_text_width(f, scale, s);
        int tx = x + (d - tw) / 2;
        int ty = y + (d - (int) f->height * scale) / 2;
        gd_draw_text(buf, pitchPx, fbw, fbh, f, scale, tx, ty, s, fg, fill, true);
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd test && make test_icons test_fonts && ./test_icons && ./test_fonts`
Expected: `test_icons OK`, `test_fonts OK`, no warnings.

- [ ] **Step 5: Commit**

```bash
git add src/ui/icons.h src/ui/icons.cpp test/test_icons.cpp
git commit -F - <<'EOF'
feat(ui): scale parameter for icon_button's letter

The letter is drawn at font scale and centred with the scaled glyph size,
so a scaled button no longer holds a 1x letter. Default scale 1 is
unchanged (exact placement now asserted).

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj
EOF
```

---

### Task 4: GlyphCanvas logical surface

**Files:**
- Modify: `src/ui/glyph_canvas.h` (header comment, add private `Scale()`)
- Modify: `src/ui/glyph_canvas.cpp` (whole file)
- Modify: `Makefile` (add `src/ui/ui_scale.o` to `OBJS`, after `src/ui/hud.o \`)

**Interfaces:**
- Consumes: `ui_scale(unsigned, unsigned)` (Task 1); `gd_scanlines(..., strength, scale)`, `gd_stipple_rect(..., color, scale)` (Task 2); `icon_button(..., f, scale)` (Task 3); `Display::Width()/Height()/Buffer()/Pitch()` (existing, physical).
- Produces: unchanged public `GlyphCanvas` API, now in **logical** units: `Width()/Height()` return physical ÷ scale; all draw calls take logical coordinates; `Text()` returns the logical pen x; `TextWidth()` is logical; `Clear()` fills the whole physical framebuffer.

This class depends on Circle's `Display` and has no host test; it is verified by the Pi 2 build (Step 4) and hardware checklist Y (Task 6).

- [ ] **Step 1: Update the header**

Replace `src/ui/glyph_canvas.h` with:

```cpp
//
// src/ui/glyph_canvas.h
//
// Bare Metal Sega Genesis
// Circle binding for the pure glyph_draw/icons primitives: draws into the
// Display's RGB565 front page. Successor to TextCanvas for redesigned screens.
//
// Coordinates are LOGICAL: the canvas divides the physical framebuffer by an
// integer UI scale (ui_scale, recomputed on every call so live video-mode
// changes apply at once) and multiplies every position, size and font scale on
// the way out. Screens lay out in Width() x Height() (always >= 640x480) and
// automatically grow at 1080p and above.
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

    unsigned Width(void) const;    // logical
    unsigned Height(void) const;   // logical

    void Clear(u16 color);         // whole physical framebuffer
    void FillRect(int x, int y, int w, int h, u16 color);
    void BlendRect(int x, int y, int w, int h, u16 color, u8 alpha);
    void Scanlines(int x, int y, int w, int h, u8 strength);
    void StippleRect(int x, int y, int w, int h, u16 color);

    // Returns the logical pen x after the last glyph.
    int  Text(const Font *f, int scale, int x, int y, const char *s,
              u16 fg, u16 bg, bool transparent);
    int  TextWidth(const Font *f, int scale, const char *s) const;   // logical

    void IconPlay (int x, int y, int size, u16 color);
    void IconTri  (int x, int y, int size, int dir, u16 color);
    void IconCross(int x, int y, int size, u16 color);
    void IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                    const Font *f);

private:
    int Scale(void) const;         // current integer UI scale (>= 1)

    Display *m_pDisplay;
};

#endif
```

- [ ] **Step 2: Rewrite the implementation**

Replace `src/ui/glyph_canvas.cpp` with:

```cpp
//
// src/ui/glyph_canvas.cpp
//
// Bare Metal Sega Genesis
// See glyph_canvas.h. Every public call maps logical -> physical by the
// current UI scale before reaching the pure primitives (which clip physically).
//
#include "glyph_canvas.h"
#include "glyph_draw.h"
#include "icons.h"
#include "ui_scale.h"

GlyphCanvas::GlyphCanvas(Display *pDisplay) : m_pDisplay(pDisplay) {}

int GlyphCanvas::Scale(void) const {
    return (int) ui_scale(m_pDisplay->Width(), m_pDisplay->Height());
}

unsigned GlyphCanvas::Width(void)  const { return m_pDisplay->Width()  / (unsigned) Scale(); }
unsigned GlyphCanvas::Height(void) const { return m_pDisplay->Height() / (unsigned) Scale(); }

void GlyphCanvas::FillRect(int x, int y, int w, int h, u16 color) {
    int s = Scale();
    gd_fill_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x * s, y * s, w * s, h * s, color);
}

void GlyphCanvas::Clear(u16 color) {
    // Physical size: logical Width()*scale can fall up to scale-1 px short.
    gd_fill_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 0, 0, (int) m_pDisplay->Width(), (int) m_pDisplay->Height(), color);
}

void GlyphCanvas::BlendRect(int x, int y, int w, int h, u16 color, u8 alpha) {
    int s = Scale();
    gd_blend_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                  (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                  x * s, y * s, w * s, h * s, color, alpha);
}

void GlyphCanvas::Scanlines(int x, int y, int w, int h, u8 strength) {
    int s = Scale();
    gd_scanlines(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x * s, y * s, w * s, h * s, strength, s);
}

void GlyphCanvas::StippleRect(int x, int y, int w, int h, u16 color) {
    int s = Scale();
    gd_stipple_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                    (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                    x * s, y * s, w * s, h * s, color, s);
}

int GlyphCanvas::Text(const Font *f, int scale, int x, int y, const char *s,
                      u16 fg, u16 bg, bool transparent) {
    int k = Scale();
    int penx = gd_draw_text(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                            (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                            f, scale * k, x * k, y * k, s, fg, bg, transparent);
    return penx / k;   // exact: every advance is a multiple of k
}

int GlyphCanvas::TextWidth(const Font *f, int scale, const char *s) const {
    return gd_text_width(f, scale, s);
}

void GlyphCanvas::IconPlay(int x, int y, int size, u16 color) {
    int s = Scale();
    icon_play(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
              (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
              x * s, y * s, size * s, color);
}

void GlyphCanvas::IconTri(int x, int y, int size, int dir, u16 color) {
    int s = Scale();
    icon_tri(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
             (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
             x * s, y * s, size * s, dir, color);
}

void GlyphCanvas::IconCross(int x, int y, int size, u16 color) {
    int s = Scale();
    icon_cross(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
               (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
               x * s, y * s, size * s, color);
}

void GlyphCanvas::IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                             const Font *f) {
    int s = Scale();
    icon_button(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                x * s, y * s, d * s, letter, fill, fg, f, s);
}
```

In `Makefile`, change:

```make
       src/ui/hud.o \
```

to:

```make
       src/ui/hud.o \
       src/ui/ui_scale.o \
```

- [ ] **Step 3: Confirm no screen code needs changes**

Run: `grep -rn "m_pDisplay->Width\|m_pDisplay->Height\|m_Display.Width\|m_Display.Height" src --include=*.cpp | grep -v "src/ui/glyph_canvas.cpp\|src/ui/text_canvas.cpp"`
Expected: only `src/video/splash_draw.cpp` lines inside `draw_image` (TextCanvas image splash, intentionally physical). Any other hit mixes physical and logical units and must be reported before continuing.

- [ ] **Step 4: Build and verify the Pi 2 kernel**

Run:
```bash
make -j$(nproc) 2>&1 | grep -E "error|warning|glyph_canvas|ui_scale|LD  "; echo "exit=${PIPESTATUS[0]}"
for f in $(find libs -name '*.a') $(find src -name '*.o') kernel7.elf; do arm-linux-gnueabihf-readelf -A "$f" 2>/dev/null | grep -m1 'Tag_CPU_arch:'; done | sort | uniq -c
```
Expected: `exit=0`, no `error`/`warning` lines, `LD    kernel7.elf` shown; every counted line is `Tag_CPU_arch: v7`. If any `v8` appears, run `make clean-all && make -j$(nproc)` and repeat.

- [ ] **Step 5: Run the host suite (primitives unaffected)**

Run: `(cd test && make run > /dev/null 2>&1; echo "exit=$?")`
Expected: `exit=0`.

- [ ] **Step 6: Commit**

```bash
git add src/ui/glyph_canvas.h src/ui/glyph_canvas.cpp Makefile
git commit -F - <<'EOF'
feat(ui): GlyphCanvas draws in logical units scaled by ui_scale

Width()/Height() report physical / ui_scale and every draw multiplies
positions, sizes and font scale (recomputed per call, so live video-mode
changes apply at once). Clear() fills the full physical framebuffer.
Menus, the typographic splash and the HUD grow 2x at 1080p with no
screen-code changes; 480p/720p are unchanged.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj
EOF
```

---

### Task 5: Remove the HUD's redundant `hud_scale`

**Files:**
- Modify: `test/test_hud.cpp` (remove the `hud_scale` block)
- Modify: `src/ui/hud.h` (remove comment, `HUD_SCALE_BASE`, declaration)
- Modify: `src/ui/hud.cpp` (remove `hud_scale`)
- Modify: `src/ui/overlay.cpp` (header comment; `Draw` and `DrawToast` geometry)

**Interfaces:**
- Consumes: `GlyphCanvas` logical API (Task 4).
- Produces: `hud_scale` and `HUD_SCALE_BASE` no longer exist. `Overlay` geometry is in logical units at scale 1 (the canvas applies the UI scale). At 1080p the HUD is still 2× physically, as before.

- [ ] **Step 1: Remove the test assertions**

In `test/test_hud.cpp`, delete these lines (and the blank line before them):

```cpp
    // hud_scale: integer scale from framebuffer height, always >= 1.
    assert(hud_scale(240)  == 1);
    assert(hud_scale(480)  == 1);
    assert(hud_scale(540)  == 1);
    assert(hud_scale(720)  == 1);
    assert(hud_scale(1080) == 2);
    assert(hud_scale(1440) == 2);
    assert(hud_scale(0)    == 1);   // guard: never zero
```

- [ ] **Step 2: Remove `hud_scale` from the HUD module**

In `src/ui/hud.h`, delete:

```cpp
// Integer render scale for the HUD, derived from the framebuffer height so the
// HUD stays a roughly constant fraction of the screen across output modes
// (native/480p/720p -> 1x, 1080p -> 2x, ...). Always >= 1.
#define HUD_SCALE_BASE 540
unsigned hud_scale(unsigned fb_height);

```

In `src/ui/hud.cpp`, delete:

```cpp
unsigned hud_scale(unsigned fb_height)
{
    unsigned s = fb_height / HUD_SCALE_BASE;
    return s < 1 ? 1 : s;
}
```

- [ ] **Step 3: Drop the scale factors in `overlay.cpp`**

Replace the comment block:

```cpp
// Geometry is derived from the font + a resolution-based integer scale
// (hud_scale), so the HUD stays a constant fraction of the screen and reads
// clearly at 720p/1080p. Base units below are at scale 1.
```

with:

```cpp
// Geometry is in GlyphCanvas logical units; the canvas applies the
// resolution-based UI scale (ui_scale), so the HUD grows with the menus and
// reads clearly at 1080p and above.
```

In `Overlay::Draw`, replace:

```cpp
    const Font *f  = kHudFont;
    int sc   = (int) hud_scale(m_pCanvas->Height());
    int fw   = (int) f->width  * sc;
    int lh   = ((int) f->height + HUD_GAP) * sc;
    int pad  = HUD_PAD * sc;
    int colW = HUD_COLCH * fw;
    int ox   = HUD_MARGIN * sc;
    int oy   = HUD_MARGIN * sc;
```

with:

```cpp
    const Font *f  = kHudFont;
    int fw   = (int) f->width;
    int lh   = (int) f->height + HUD_GAP;
    int pad  = HUD_PAD;
    int colW = HUD_COLCH * fw;
    int ox   = HUD_MARGIN;
    int oy   = HUD_MARGIN;
```

and in the same function replace both `m_pCanvas->Text(f, sc, ` occurrences with `m_pCanvas->Text(f, 1, `.

In `Overlay::DrawToast`, replace:

```cpp
    int sc = (int) hud_scale((unsigned) H);

    int padX = 12 * sc, padY = 6 * sc;
    int textW = m_pCanvas->TextWidth(f, sc, m_Toast);
    int boxW  = textW + 2 * padX;
    int boxH  = (int) f->height * sc + 2 * padY;
    int x = (W - boxW) / 2; if (x < 0) x = 0;
    int y = H - boxH - 36 * sc;  // near bottom (HUD is top-left)
```

with:

```cpp
    int padX = 12, padY = 6;
    int textW = m_pCanvas->TextWidth(f, 1, m_Toast);
    int boxW  = textW + 2 * padX;
    int boxH  = (int) f->height + 2 * padY;
    int x = (W - boxW) / 2; if (x < 0) x = 0;
    int y = H - boxH - 36;  // near bottom (HUD is top-left)
```

and replace `    m_pCanvas->Text(f, sc, x + padX, y + padY, m_Toast,` with `    m_pCanvas->Text(f, 1, x + padX, y + padY, m_Toast,`.

- [ ] **Step 4: Verify nothing references the removed API, tests pass, kernel builds**

Run:
```bash
grep -rn "hud_scale\|HUD_SCALE_BASE\|\bsc\b" src/ui/overlay.cpp src/ui/hud.h src/ui/hud.cpp test/test_hud.cpp; echo "grep-exit=$?"
cd test && make test_hud && ./test_hud && cd ..
make -j$(nproc) 2>&1 | grep -E "error|warning|overlay|hud|LD  "; echo "exit=${PIPESTATUS[0]}"
```
Expected: `grep-exit=1` (no matches); `All hud tests passed`; build `exit=0` with no `error`/`warning` lines.

- [ ] **Step 5: Commit**

```bash
git add test/test_hud.cpp src/ui/hud.h src/ui/hud.cpp src/ui/overlay.cpp
git commit -F - <<'EOF'
refactor(ui): drop hud_scale now that GlyphCanvas scales the HUD

The canvas's logical height is always < 960, so hud_scale always returned
1. Overlay geometry is now plain logical units: <=720p unchanged, 1080p
still 2x (same physical size and per-frame cost), 1440p 3x like menus.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj
EOF
```

---

### Task 6: Hardware checklist Y, full verification, merge

**Files:**
- Create: `docs/hardware-checklist-ui-scaling.md`

**Interfaces:**
- Consumes: everything above.
- Produces: checklist Y for hardware verification; `feat/ui-scaling` merged into `main` (not pushed).

- [ ] **Step 1: Write the checklist**

Create `docs/hardware-checklist-ui-scaling.md`:

```markdown
# Hardware Checklist — UI Scaling by Output Resolution (checklist Y)

Scale rule: `max(1, min(height / 480, width / 640))` — 480p/720p 1x, 1080p 2x.
Switch modes in Settings > Video Mode (confirm-or-revert).

- [ ] Y1. 480p and 720p: every screen (ROM browser, pause menu, Settings and
      all sub-screens, HUD, toasts, boot splash) looks exactly as before.
- [ ] Y2. 1080p: ROM browser at 2x (about 13 rows) and scrolls correctly
      through a long list.
- [ ] Y3. 1080p: pause menu at 2x with the dimmed game behind it and scanlines;
      moving the selector does not lighten the background.
- [ ] Y4. 1080p: Settings and every sub-screen (Controls, Hotkeys, Video Mode,
      Calibrate, Test Controllers) readable and fully on screen; Controller
      Test shows all four panels and the footer.
- [ ] Y5. Live switch 720p -> 1080p in Video Mode rescales immediately; the
      confirm-or-revert dialog is readable at the new size; reverting returns
      to 1x.
- [ ] Y6. 1080p in game: HUD and toasts the same size as before this change;
      FPS with the HUD on unchanged, no new audio underruns.
- [ ] Y7. Native mode on a 1080p TV behaves like 1080p (2x).
- [ ] Y8. Typographic boot splash is centred and readable at 1080p.
```

- [ ] **Step 2: Full host suite**

Run: `cd test && make run 2>&1 | grep -E "FAIL|Assert|rror" ; make run > /dev/null 2>&1; echo "exit=$?"`
Expected: no output from the grep, `exit=0`.

- [ ] **Step 3: Audit scope**

Run: `git diff main --stat -- src/menu src/ui/screen_chrome.cpp src/ui/screen_chrome.h src/video src/ui/text_canvas.cpp`
Expected: empty (no screen, splash, Display or TextCanvas changes).

- [ ] **Step 4: Clean Pi 2 build with architecture check**

Run:
```bash
make -j$(nproc) 2>&1 | grep -E "error|warning"; echo "exit=${PIPESTATUS[0]}"
for f in $(find libs -name '*.a') $(find src -name '*.o') kernel7.elf; do arm-linux-gnueabihf-readelf -A "$f" 2>/dev/null | grep -m1 'Tag_CPU_arch:'; done | sort | uniq -c
grep -m1 " _end = " kernel7.map
```
Expected: `exit=0`, no warnings, all `Tag_CPU_arch: v7`, `_end` below `0x01408000`.

- [ ] **Step 5: Commit the checklist**

```bash
git add docs/hardware-checklist-ui-scaling.md
git commit -F - <<'EOF'
docs: hardware checklist Y for UI scaling

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj
EOF
```

- [ ] **Step 6: Merge into main (no push)**

```bash
git checkout main
git merge --no-ff feat/ui-scaling -m "Merge branch 'feat/ui-scaling'

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01HhjqPhwsAQofNQzYjAJgaj"
git log --oneline -3
```
Expected: merge commit on `main`; working tree clean apart from the untracked `build/` and `dist/` left by `make dist`.
