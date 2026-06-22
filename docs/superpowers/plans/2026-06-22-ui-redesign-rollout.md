# On-Screen UI Redesign Rollout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Roll the v1.0.0 design language (already shipped on the ROM browser) out to the remaining screens — splash, pause menu, save-state slots, settings, video mode, controls, and the hotkey/calibration cluster — via a shared `screen_chrome` helper.

**Architecture:** Reuse the existing `GlyphCanvas` + `theme.h` + baked fonts. Add a `screen_chrome` helper (Circle-side) for the repeated header / footer / selectable value-row / hint-item patterns so every screen matches. Migrate each screen from `TextCanvas*` to `GlyphCanvas*`, flipping its kernel constructor line in the same task so the build stays green. The in-game diagnostics HUD (`Overlay`) stays on `TextCanvas` for now, so `TextCanvas` is retained (not retired this pass).

**Tech Stack:** C++ bare-metal (Circle, Pi 2, AArch32), RGB565 framebuffer, host C++ unit tests for pure additions.

## Global Constraints

- **Decisions (this rollout):** splash → adopt the typographic mock splash (default); boot diagnostics (01) → DEFERRED, not built here.
- **Screens in scope:** 00 splash, 04 pause, 05 settings, 06 video mode, 07 controls, 08 save-state slots; plus hotkey + calibration (not in mock — themed for consistency since they share the canvas).
- **Render-only:** input handling, navigation, persistence, and control flow for each screen stay exactly as they are — only the `Render`/draw code and the canvas type change.
- **No snprintf on bare metal** — use manual digit formatting (the existing `fmt_volume`/`hex4` helpers stay; reuse their style).
- **Palette/fonts:** use `theme::*` and `g_font_ps2p8` (titles, integer-scaled) / `g_font_vt323_22` (body). No raw hex literals in new code.
- **Layout idiom (match ROM browser):** `PAD=40, TOP=36, ROW_H=30, LIST_TOP=96, FOOT_H=40` px at 1080p; all draws clip, so smaller modes are safe. Scanlines drawn last (`strength 60`).
- **Build:** kernel cross-build `make` (produces `kernel7.img`); host tests `make -C test run`. Each task ends green on both where applicable.
- **Kernel OBJS** are listed explicitly in root `Makefile`; new `.cpp` must be added there.
- **Member init order:** `m_GlyphCanvas` is declared after `m_Display` and before the screens, so passing `&m_GlyphCanvas` to screen constructors is safe.

---

### Task 1: Triangle icon (`icon_tri`) for adjust arrows + chevrons

**Files:**
- Modify: `src/ui/icons.h`, `src/ui/icons.cpp`
- Modify: `test/test_icons.cpp`

**Interfaces:**
- Produces: `void icon_tri(uint16_t *buf, unsigned pitchPx, int fbw, int fbh, int x, int y, int size, int dir, uint16_t color);` — filled triangle in a `size`×`size` box. `dir`: 0=right, 1=left, 2=down. `icon_play` becomes a thin alias for `dir=0`.

- [ ] **Step 1: Extend the test**

Add to `test/test_icons.cpp` `main()` (before the final `printf`):
```cpp
    // left triangle: base at right edge, tip at left-middle
    reset();
    icon_tri(fb, W, W, H, 0, 0, 8, 1, 0x07E0);
    assert(fb[4 * W + 7] == 0x07E0);          // right edge mid = base
    assert(fb[0 * W + 0] == 0x0000);          // top-left empty (tapered)

    // down triangle: base at top, tip at bottom-middle
    reset();
    icon_tri(fb, W, W, H, 0, 0, 8, 2, 0x07E0);
    assert(fb[0 * W + 4] == 0x07E0);          // top-middle = base
    assert(fb[7 * W + 0] == 0x0000);          // bottom-left empty
```

- [ ] **Step 2: Run to verify it fails**

Run: `make -C test test_icons`
Expected: FAIL — `icon_tri` not declared.

- [ ] **Step 3: Add the declaration**

In `src/ui/icons.h`, after the `icon_play` declaration add:
```cpp
// Filled triangle in a size x size box. dir: 0=right, 1=left, 2=down.
void icon_tri(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
              int x, int y, int size, int dir, uint16_t color);
```

- [ ] **Step 4: Implement and re-point `icon_play`**

In `src/ui/icons.cpp`, replace the existing `icon_play` function with:
```cpp
void icon_tri(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
              int x, int y, int size, int dir, uint16_t color) {
    int mid = size / 2;
    if (dir == 2) {                       // down: rows widen at top, taper down
        for (int r = 0; r < size; r++) {
            int w = size - r;
            if (w < 1) w = 1;
            int cx = x + (size - w) / 2;
            gd_fill_rect(buf, pitchPx, fbw, fbh, cx, y + r, w, 1, color);
        }
        return;
    }
    for (int r = 0; r < size; r++) {      // right / left: width tapers to a point
        int dist = (r < mid) ? r : (size - 1 - r);
        int w = dist; if (w < 1) w = 1;
        int rx = (dir == 1) ? (x + size - w) : x;   // left -> base at right edge
        gd_fill_rect(buf, pitchPx, fbw, fbh, rx, y + r, w, 1, color);
    }
}

void icon_play(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
               int x, int y, int size, uint16_t color) {
    icon_tri(buf, pitchPx, fbw, fbh, x, y, size, 0, color);
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `make -C test test_icons && ./test/test_icons`
Expected: `test_icons OK`

- [ ] **Step 6: Commit**
```bash
git add src/ui/icons.h src/ui/icons.cpp test/test_icons.cpp
git commit -m "feat(ui): icon_tri (directional triangle) for adjust arrows/chevrons"
```

---

### Task 2: Shared screen chrome (`screen_chrome`)

**Files:**
- Create: `src/ui/screen_chrome.h`, `src/ui/screen_chrome.cpp`
- Modify: `src/ui/glyph_canvas.h`, `src/ui/glyph_canvas.cpp` (add `IconTri`)
- Modify: root `Makefile` (add `src/ui/screen_chrome.o`)

No host test (thin Circle drawing); verified by the kernel build in later tasks and the first screen that uses it.

**Interfaces:**
- Consumes: `GlyphCanvas`, `theme`, `g_font_ps2p8`, `g_font_vt323_22`, `icon_*`.
- Produces (in `namespace chrome`):
  - constants `PAD, TOP, ROW_H, LIST_TOP, FOOT_H`
  - `void header(GlyphCanvas *c, const char *title, const char *right, u16 rightColor);`
  - `void footer_divider(GlyphCanvas *c);`
  - `void scanlines(GlyphCanvas *c);`
  - `int  value_row(GlyphCanvas *c, int i, bool sel, const char *label, const char *value, u16 valueColor, bool arrows);` (returns the row's top y)
  - `int  hint_dpad(GlyphCanvas *c, int x, int y, const char *label);` (returns next x)
  - `int  hint_lr(GlyphCanvas *c, int x, int y, const char *label);`
  - `int  hint_button(GlyphCanvas *c, int x, int y, char btn, u16 fill, const char *label);`
- Also adds `void GlyphCanvas::IconTri(int x, int y, int size, int dir, u16 color);`

- [ ] **Step 1: Add `IconTri` to GlyphCanvas**

In `src/ui/glyph_canvas.h`, after `IconPlay`:
```cpp
    void IconTri (int x, int y, int size, int dir, u16 color);
```
In `src/ui/glyph_canvas.cpp`, after `IconPlay`'s definition:
```cpp
void GlyphCanvas::IconTri(int x, int y, int size, int dir, u16 color) {
    icon_tri(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
             (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
             x, y, size, dir, color);
}
```

- [ ] **Step 2: Write `screen_chrome.h`**

`src/ui/screen_chrome.h`:
```cpp
//
// src/ui/screen_chrome.h
//
// Bare Metal Sega Genesis
// Shared draws for the redesigned menu screens (header, footer hints, value
// rows, scanlines) so every screen matches the v1.0.0 design. Circle-side.
//
#ifndef _ui_screen_chrome_h
#define _ui_screen_chrome_h
#include <circle/types.h>
#include "glyph_canvas.h"

namespace chrome {
    static const int PAD      = 40;
    static const int TOP      = 36;
    static const int ROW_H    = 30;
    static const int LIST_TOP = 96;
    static const int FOOT_H   = 40;

    void header(GlyphCanvas *c, const char *title, const char *right, u16 rightColor);
    void footer_divider(GlyphCanvas *c);
    void scanlines(GlyphCanvas *c);

    // Selectable label/value row (row index i, 0-based, under LIST_TOP).
    // value right-aligned; arrows draws amber < > around it. Returns row top y.
    int value_row(GlyphCanvas *c, int i, bool sel, const char *label,
                  const char *value, u16 valueColor, bool arrows);

    // Footer hint items; each returns the x just past what it drew.
    int hint_dpad  (GlyphCanvas *c, int x, int y, const char *label);
    int hint_lr    (GlyphCanvas *c, int x, int y, const char *label);
    int hint_button(GlyphCanvas *c, int x, int y, char btn, u16 fill, const char *label);
}

#endif
```

- [ ] **Step 3: Write `screen_chrome.cpp`**

`src/ui/screen_chrome.cpp`:
```cpp
//
// src/ui/screen_chrome.cpp
//
// Bare Metal Sega Genesis
// See screen_chrome.h.
//
#include "screen_chrome.h"
#include "theme.h"
#include "fonts/font_ps2p8.h"
#include "fonts/font_vt323_22.h"

namespace chrome {

void header(GlyphCanvas *c, const char *title, const char *right, u16 rightColor) {
    int W = (int) c->Width();
    c->Text(&g_font_ps2p8, 2, PAD, TOP, title, theme::WHITE, theme::BG, true);
    if (right && right[0]) {
        int rw = c->TextWidth(&g_font_vt323_22, 1, right);
        c->Text(&g_font_vt323_22, 1, W - PAD - rw, TOP, right, rightColor,
                theme::BG, true);
    }
    c->FillRect(PAD, TOP + 34, W - 2 * PAD, 2, theme::TEXT_DIM);
}

void footer_divider(GlyphCanvas *c) {
    int W = (int) c->Width();
    int H = (int) c->Height();
    c->FillRect(PAD, H - FOOT_H - 2, W - 2 * PAD, 2, theme::TEXT_DIM);
}

void scanlines(GlyphCanvas *c) {
    c->Scanlines(0, 0, (int) c->Width(), (int) c->Height(), 60);
}

int value_row(GlyphCanvas *c, int i, bool sel, const char *label,
              const char *value, u16 valueColor, bool arrows) {
    int W = (int) c->Width();
    int y = LIST_TOP + i * ROW_H;
    int rowW = W - 2 * PAD;
    if (sel) c->FillRect(PAD - 6, y - 2, rowW + 12, ROW_H, theme::SELECTION);

    u16 lc = sel ? theme::WHITE : theme::TEXT;
    c->Text(&g_font_vt323_22, 1, PAD + 4, y, label, lc, theme::BG, true);

    if (value && value[0]) {
        int vw = c->TextWidth(&g_font_vt323_22, 1, value);
        u16 vc = sel ? theme::WHITE : valueColor;
        u16 ac = sel ? theme::WHITE : theme::ADJUST;
        if (arrows) {
            int rax = W - PAD - 12;          // right arrow box
            int vx  = rax - 6 - vw;          // value sits left of it
            int lax = vx - 18;               // left arrow box
            c->IconTri(lax, y + 5, 12, 1, ac);
            c->Text(&g_font_vt323_22, 1, vx, y, value, vc, theme::BG, true);
            c->IconTri(rax, y + 5, 12, 0, ac);
        } else {
            c->Text(&g_font_vt323_22, 1, W - PAD - vw, y, value, vc,
                    theme::BG, true);
        }
    }
    return y;
}

int hint_dpad(GlyphCanvas *c, int x, int y, const char *label) {
    c->IconCross(x, y, 16, theme::TEXT);
    c->Text(&g_font_ps2p8, 1, x + 24, y + 4, label, theme::TEXT_DIM, theme::BG, true);
    return x + 24 + c->TextWidth(&g_font_ps2p8, 1, label) + 30;
}

int hint_lr(GlyphCanvas *c, int x, int y, const char *label) {
    c->IconTri(x, y + 4, 12, 1, theme::ADJUST);
    c->IconTri(x + 16, y + 4, 12, 0, theme::ADJUST);
    c->Text(&g_font_ps2p8, 1, x + 36, y + 4, label, theme::TEXT_DIM, theme::BG, true);
    return x + 36 + c->TextWidth(&g_font_ps2p8, 1, label) + 30;
}

int hint_button(GlyphCanvas *c, int x, int y, char btn, u16 fill, const char *label) {
    c->IconButton(x, y, 16, btn, fill, theme::WHITE, &g_font_ps2p8);
    c->Text(&g_font_ps2p8, 1, x + 24, y + 4, label, theme::TEXT_DIM, theme::BG, true);
    return x + 24 + c->TextWidth(&g_font_ps2p8, 1, label) + 30;
}

}  // namespace chrome
```

- [ ] **Step 4: Register the object**

In root `Makefile` OBJS, after `src/ui/glyph_canvas.o`:
```
       src/ui/screen_chrome.o \
```

- [ ] **Step 5: Build the kernel**

Run: `make`
Expected: builds `kernel7.img` (screen_chrome + IconTri compile and link).

- [ ] **Step 6: Commit**
```bash
git add src/ui/screen_chrome.h src/ui/screen_chrome.cpp src/ui/glyph_canvas.h src/ui/glyph_canvas.cpp Makefile
git commit -m "feat(ui): shared screen_chrome (header/footer/value-row/hints) + GlyphCanvas::IconTri"
```

---

### Task 3: Typographic splash (00)

**Files:**
- Modify: `src/video/splash.h` (declare text splash)
- Modify: `src/video/splash_draw.cpp` (add text splash)
- Modify: `src/kernel.cpp` (call text splash at boot)

**Interfaces:**
- Consumes: `GlyphCanvas`, `theme`, fonts.
- Produces: `void splash_show_text(GlyphCanvas *canvas, Display *display);`

- [ ] **Step 1: Declare in `splash.h`**

In `src/video/splash.h`, after the existing `class Display;` / `class TextCanvas;` forward decls add:
```cpp
class GlyphCanvas;
```
and after `splash_show_embedded`:
```cpp
void splash_show_text(GlyphCanvas *canvas, Display *display);
```

- [ ] **Step 2: Implement in `splash_draw.cpp`**

Add includes at the top of `src/video/splash_draw.cpp`:
```cpp
#include "../ui/glyph_canvas.h"
#include "../ui/theme.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
```
Append this function:
```cpp
// Typographic boot splash (Claude Design v1.0.0): centered wordmark.
void splash_show_text(GlyphCanvas *canvas, Display *display)
{
    if (display->Buffer() == 0) return;
    int W = (int) canvas->Width();
    int H = (int) canvas->Height();
    canvas->Clear(theme::BG);

    const char *kicker = "BARE . METAL";
    int kw = canvas->TextWidth(&g_font_ps2p8, 2, kicker);
    canvas->Text(&g_font_ps2p8, 2, W / 2 - kw / 2, H / 2 - 120, kicker,
                 theme::VALUE, theme::BG, true);

    const char *title = "GENESIS";
    int tw = canvas->TextWidth(&g_font_ps2p8, 6, title);
    canvas->Text(&g_font_ps2p8, 6, W / 2 - tw / 2, H / 2 - 70, title,
                 theme::WHITE, theme::BG, true);

    canvas->FillRect(W / 2 - 80, H / 2 + 10, 160, 4, theme::SELECTION);

    const char *sub = "EMULATOR";
    int sw = canvas->TextWidth(&g_font_vt323_22, 1, sub);
    canvas->Text(&g_font_vt323_22, 1, W / 2 - sw / 2 - 30, H / 2 + 36, sub,
                 theme::TEXT_MUTED, theme::BG, true);
    canvas->IconButton(W / 2 + sw / 2 - 10, H / 2 + 34, 18, '6',
                       theme::ADJUST, theme::BG, &g_font_ps2p8);

    const char *press = "PRESS START";
    int pw = canvas->TextWidth(&g_font_ps2p8, 2, press);
    canvas->Text(&g_font_ps2p8, 2, W / 2 - pw / 2, H - 180, press,
                 theme::WHITE, theme::BG, true);

    const char *foot = "RASPBERRY PI . CIRCLE . LIBRETRO / GENESIS PLUS GX";
    int fw = canvas->TextWidth(&g_font_vt323_22, 1, foot);
    canvas->Text(&g_font_vt323_22, 1, W / 2 - fw / 2, H - 120, foot,
                 theme::TEXT_DIM, theme::BG, true);

    canvas->Scanlines(0, 0, W, H, 60);
}
```

- [ ] **Step 3: Call it at boot**

In `src/kernel.cpp` line ~120, replace:
```cpp
			splash_show_embedded (&m_Canvas, &m_Display);
```
with:
```cpp
			splash_show_text (&m_GlyphCanvas, &m_Display);
```
Leave the `splash_apply_override (&m_Storage, &m_Canvas, &m_Display);` call (line ~158) as-is — an SD `splash.raw` image still overrides if the user supplies one.

- [ ] **Step 4: Build**

Run: `make`
Expected: builds `kernel7.img`.

- [ ] **Step 5: Commit**
```bash
git add src/video/splash.h src/video/splash_draw.cpp src/kernel.cpp
git commit -m "feat(video): typographic boot splash (design v1.0.0) via GlyphCanvas"
```

---

### Task 4: Pause menu (04) — dim-behind panel

**Files:**
- Modify: `src/menu/pause_menu.h` (canvas type), `src/menu/pause_menu.cpp` (`Render` + `Message`)
- Modify: `src/kernel.cpp` (constructor line 61)

Render-only + canvas type. `PickSlot` is re-skinned in Task 5; this task keeps it compiling by using `GlyphCanvas` calls there too (minimal: it still uses `Width/Height/FillRect/Text` — but it currently uses `CharW/CharH/DrawText`, which GlyphCanvas lacks). **So this task also rewrites `PickSlot`'s drawing minimally to GlyphCanvas primitives; Task 5 then restyles it fully.** To avoid double work, Task 5 is merged here: this task delivers the pause menu AND the slot picker.

**Interfaces:** Consumes `chrome`, `theme`, fonts, `BlendRect`.

- [ ] **Step 1: Update `pause_menu.h`**

Replace `#include "../ui/text_canvas.h"` with `#include "../ui/glyph_canvas.h"`; change the constructor param `TextCanvas *pCanvas` → `GlyphCanvas *pCanvas`; change the member `TextCanvas *m_pCanvas;` → `GlyphCanvas *m_pCanvas;`.

- [ ] **Step 2: Rewrite the top, `Render`, `Message`, and `PickSlot` in `pause_menu.cpp`**

Replace the color constants block (`BOX/WHITE/GREY/SELFG/SELBG`) and add includes:
```cpp
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
```
(Keep the existing `#include "pause_menu.h"`, `settings_screen.h`, `menu_state.h`, `joypad_map.h`, `<circle/timer.h>`.)

Replace `Render` with:
```cpp
void PauseMenu::Render(int selected)
{
    using namespace chrome;
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();

    // Dim the live game frame behind the panel (don't wipe to black).
    m_pCanvas->BlendRect(0, 0, W, H, theme::BG, 200);

    int panelW = 460;
    int panelH = NUM_ENTRIES * ROW_H + 120;
    int px = W / 2 - panelW / 2;
    int py = H / 2 - panelH / 2;
    m_pCanvas->FillRect(px, py, panelW, panelH, theme::BG);
    m_pCanvas->FillRect(px, py, panelW, 3, theme::SELECTION);          // top accent
    m_pCanvas->FillRect(px, py + panelH - 3, panelW, 3, theme::SELECTION);

    const char *title = "|| PAUSED";
    int tw = m_pCanvas->TextWidth(&g_font_ps2p8, 2, title);
    m_pCanvas->Text(&g_font_ps2p8, 2, W / 2 - tw / 2, py + 20, title,
                    theme::WHITE, theme::BG, true);

    for (int i = 0; i < NUM_ENTRIES; i++)
    {
        int ty = py + 64 + i * ROW_H;
        bool sel = (i == selected);
        if (sel) m_pCanvas->FillRect(px + 16, ty - 2, panelW - 32, ROW_H,
                                     theme::SELECTION);
        if (sel) m_pCanvas->IconPlay(px + 22, ty + 4, 16, theme::WHITE);
        m_pCanvas->Text(&g_font_vt323_22, 1, px + 48, ty, LABELS[i],
                        sel ? theme::WHITE : theme::TEXT, theme::BG, true);
    }

    int fy = py + panelH - 28;
    int hx = hint_dpad(m_pCanvas, px + 20, fy - 4, "MOVE");
    hint_button(m_pCanvas, hx, fy - 4, 'A', theme::SELECTION, "SELECT");
    m_pCanvas->Scanlines(0, 0, W, H, 60);
}
```

Replace `Message` with:
```cpp
void PauseMenu::Message(const char *text)
{
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();
    int bw = 460, bh = 70;
    int bx = W / 2 - bw / 2, by = H / 2 - bh / 2;
    m_pCanvas->FillRect(bx, by, bw, bh, theme::BG);
    m_pCanvas->FillRect(bx, by, bw, 3, theme::SELECTION);
    m_pCanvas->Text(&g_font_vt323_22, 1, bx + 24, by + 24, text,
                    theme::TEXT, theme::BG, true);
    CTimer::SimpleMsDelay(1200);
}
```

Replace `PickSlot`'s drawing block (the `if (redraw) { … }` body) with the themed slot picker; keep all input logic unchanged:
```cpp
        if (redraw)
        {
            int W = (int) m_pCanvas->Width();
            int H = (int) m_pCanvas->Height();
            m_pCanvas->BlendRect(0, 0, W, H, theme::BG, 200);

            int panelW = 460, panelH = NUM_SLOTS * 44 + 120;
            int px = W / 2 - panelW / 2, py = H / 2 - panelH / 2;
            m_pCanvas->FillRect(px, py, panelW, panelH, theme::BG);
            m_pCanvas->FillRect(px, py, panelW, 3, theme::SELECTION);

            const char *ttl = forLoad ? "LOAD STATE" : "SAVE STATE";
            int tw = m_pCanvas->TextWidth(&g_font_ps2p8, 2, ttl);
            m_pCanvas->Text(&g_font_ps2p8, 2, W / 2 - tw / 2, py + 18, ttl,
                            theme::WHITE, theme::BG, true);

            for (int i = 0; i < NUM_SLOTS; i++)
            {
                int ty = py + 64 + i * 44;
                bool cur = (i == sel) && selable[i];
                if (cur) m_pCanvas->FillRect(px + 16, ty - 2, panelW - 32, 38,
                                             theme::SELECTION);
                if (cur) m_pCanvas->IconPlay(px + 22, ty + 6, 16, theme::WHITE);

                char label[8] = { 'S','l','o','t',' ', (char)('1'+i), '\0' };
                u16 lc = cur ? theme::WHITE : (selable[i] ? theme::TEXT : theme::TEXT_DIM);
                m_pCanvas->Text(&g_font_vt323_22, 1, px + 48, ty, label,
                                lc, theme::BG, true);

                // USED / EMPTY badge, right-aligned.
                const char *badge = occupied[i] ? "USED" : "EMPTY";
                u16 bf = occupied[i] ? theme::ACTIVE : theme::TEXT_DIM;
                int bw = m_pCanvas->TextWidth(&g_font_ps2p8, 1, badge);
                if (occupied[i])
                    m_pCanvas->FillRect(px + panelW - 40 - bw - 12, ty + 2,
                                        bw + 16, 22, theme::ACTIVE);
                m_pCanvas->Text(&g_font_ps2p8, 1, px + panelW - 40 - bw - 4, ty + 6,
                                badge, occupied[i] ? theme::BG : bf, theme::BG, true);
            }

            int fy = py + panelH - 28;
            int hx = hint_button(m_pCanvas, px + 20, fy - 4, 'A', theme::SELECTION,
                                 forLoad ? "LOAD" : "SAVE");
            hint_button(m_pCanvas, hx, fy - 4, 'B', theme::TEXT_DIM, "CANCEL");
            m_pCanvas->Scanlines(0, 0, W, H, 60);
            redraw = false;
        }
```
Add `using namespace chrome;` is not needed here (PickSlot uses `hint_button` qualified). Use `chrome::hint_button` instead, or add `using namespace chrome;` at the top of `PickSlot`. Use the qualified form: replace `hint_button(` with `chrome::hint_button(` in this block.

- [ ] **Step 3: Flip the kernel constructor**

In `src/kernel.cpp` line ~61 change `m_PauseMenu (&m_Canvas, …)` to `m_PauseMenu (&m_GlyphCanvas, …)`.

- [ ] **Step 4: Build**

Run: `make`
Expected: builds `kernel7.img`.

- [ ] **Step 5: Commit**
```bash
git add src/menu/pause_menu.h src/menu/pause_menu.cpp src/kernel.cpp
git commit -m "feat(menu): re-skin pause menu + save-state slot picker (dim-behind panel)"
```

---

### Task 5: Settings (05)

**Files:**
- Modify: `src/menu/settings_screen.h` (canvas type), `src/menu/settings_screen.cpp` (`Render` + `fmt_volume`)
- Modify: `src/kernel.cpp` (constructor line 60)

**Interfaces:** Consumes `chrome`, `theme`, fonts.

- [ ] **Step 1: Update `settings_screen.h`**

Replace `#include "../ui/text_canvas.h"` with `#include "../ui/glyph_canvas.h"`; change constructor param and the `TextCanvas *m_pCanvas;` member to `GlyphCanvas`.

- [ ] **Step 2: Rewrite values + `Render` in `settings_screen.cpp`**

Add includes:
```cpp
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
```
Remove the `BOX/WHITE/SELFG/SELBG` constants. Replace `fmt_volume` to produce a plain number (arrows now come from `value_row`):
```cpp
static void fmt_volume(char *out, unsigned v)
{
    char rev[4]; int n = 0;
    if (v == 0) rev[n++] = '0';
    else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
    int i = 0; while (n) out[i++] = rev[--n]; out[i] = '\0';
}
```
Replace `Render` with (value strings lose their `< >`; `value_row` adds amber arrows for the 12 adjustable rows; rows 12–15 are sub-screens with a dim chevron):
```cpp
void SettingsScreen::Render(int selected)
{
    using namespace chrome;
    char volVal[8];
    fmt_volume(volVal, m_pSettings->volume);
    const char *scaleVal =
        m_pSettings->scale_mode == ScaleMode::Stretch ? "Stretch" :
        m_pSettings->scale_mode == ScaleMode::Aspect  ? "Aspect"  : "Integer";
    const char *wideVal  = m_pSettings->widescreen ? "On" : "Off";
    const char *muteVal  = m_pSettings->mute ? "On" : "Off";
    const char *regionVal =
        m_pSettings->region == Region::NTSC ? "NTSC" :
        m_pSettings->region == Region::PAL  ? "PAL"  : "Auto";
    bool autoOn = m_pRomPath != 0 &&
                  strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
    const char *autoVal = autoOn ? "On" : "Off";
    const char *hotkeyVal =
        m_pSettings->menu_hotkey == MenuHotkey::StartA ? "Start+A" :
        m_pSettings->menu_hotkey == MenuHotkey::StartB ? "Start+B" :
        m_pSettings->menu_hotkey == MenuHotkey::LR     ? "L+R"     : "Start+Select";
    const char *audioVal =
        m_pSettings->audio_output == AudioOutput::Analog ? "Analog"  :
        m_pSettings->audio_output == AudioOutput::I2S    ? "I2S DAC" : "HDMI";
    const char *vsyncVal = m_pSettings->vsync ? "On" : "Off";
    const char *dbgVal   = m_pSettings->debug_overlay ? "On" : "Off";
    const char *latVal =
        m_pSettings->audio_latency == AudioLatency::Low  ? "Low"  :
        m_pSettings->audio_latency == AudioLatency::High ? "High" : "Medium";
    const char *padVal = m_pSettings->pad_type == PadType::ThreeButton
                             ? "3-button" : "6-button";

    const char *labels[NUM_ROWS] = { "Video Scale", "Widescreen", "Volume",
        "Mute", "Region", "Auto-Launch ROM", "Menu Hotkey", "Audio Out",
        "Vsync", "Debug Overlay", "Audio Latency", "Pad Type",
        "Controls", "Video Mode", "Hotkeys", "Calibrate Controller" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
        regionVal, autoVal, hotkeyVal, audioVal, vsyncVal, dbgVal, latVal,
        padVal, "", "", "", "" };

    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "SETTINGS", "SD:/settings.txt", theme::VALUE);

    for (int i = 0; i < NUM_ROWS; i++)
    {
        bool sel = (i == selected);
        if (i < 12) {
            value_row(m_pCanvas, i, sel, labels[i], values[i], theme::VALUE, true);
        } else {
            int y = value_row(m_pCanvas, i, sel, labels[i], "", theme::VALUE, false);
            int W = (int) m_pCanvas->Width();
            m_pCanvas->IconTri(W - PAD - 14, y + 5, 12, 0,
                               sel ? theme::WHITE : theme::TEXT_DIM);   // chevron
        }
    }

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_dpad(m_pCanvas, PAD, fy, "NAVIGATE");
    hx = hint_lr(m_pCanvas, hx, fy, "CHANGE");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
}
```

- [ ] **Step 3: Flip the kernel constructor**

In `src/kernel.cpp` line ~60 change `m_SettingsScreen (&m_Canvas, …)` to `m_SettingsScreen (&m_GlyphCanvas, …)`.

- [ ] **Step 4: Build**

Run: `make`
Expected: builds `kernel7.img`.

- [ ] **Step 5: Commit**
```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp src/kernel.cpp
git commit -m "feat(menu): re-skin settings screen (value rows + chevrons + hints)"
```

---

### Task 6: Video mode (06) — mode strip + confirm/revert

**Files:**
- Modify: `src/menu/video_mode_screen.h` (canvas type), `src/menu/video_mode_screen.cpp` (`Render`, `Confirm`, the inline "Mode unavailable" message)
- Modify: `src/kernel.cpp` (constructor line 55)

- [ ] **Step 1: Update `video_mode_screen.h`**

Swap include + constructor param + member to `GlyphCanvas`.

- [ ] **Step 2: Rewrite drawing in `video_mode_screen.cpp`**

Add includes:
```cpp
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
```
Remove `BOX/WHITE/SELFG/SELBG`. Replace `Render` with a mode-chip strip:
```cpp
void VideoModeScreen::Render(VideoMode sel)
{
    using namespace chrome;
    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "VIDEO MODE", "HDMI OUTPUT", theme::VALUE);

    const VideoMode modes[NUM_MODES] = { VideoMode::Native, VideoMode::P1080,
                                         VideoMode::P720, VideoMode::P480 };
    int x = PAD;
    int y = LIST_TOP;
    for (int i = 0; i < NUM_MODES; i++)
    {
        const char *lbl = mode_label(modes[i]);
        int tw = m_pCanvas->TextWidth(&g_font_vt323_22, 1, lbl);
        int chipW = tw + 28;
        bool on = (modes[i] == sel);
        if (on) m_pCanvas->FillRect(x, y, chipW, 30, theme::SELECTION);
        else    { m_pCanvas->FillRect(x, y, chipW, 1, theme::TEXT_DIM);
                  m_pCanvas->FillRect(x, y + 29, chipW, 1, theme::TEXT_DIM); }
        m_pCanvas->Text(&g_font_vt323_22, 1, x + 14, y + 4, lbl,
                        on ? theme::WHITE : theme::TEXT_MUTED, theme::BG, true);
        x += chipW + 12;
    }

    m_pCanvas->Text(&g_font_vt323_22, 1, PAD, y + 50,
                    "Only a confirmed mode is saved; an unsupported signal auto-reverts.",
                    theme::TEXT_DIM, theme::BG, true);

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_lr(m_pCanvas, PAD, fy, "SELECT");
    hx = hint_button(m_pCanvas, hx, fy, 'A', theme::SELECTION, "APPLY");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
}
```
Replace the body of `Confirm` (the per-second redraw block) with a themed amber box + progress bar; keep the timing/poll logic:
```cpp
boolean VideoModeScreen::Confirm(void)
{
    using namespace chrome;
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();
    unsigned prev = m_pGamepad->MenuButtons();

    for (int sec = 15; sec > 0; sec--)
    {
        int bw = 560, bh = 200;
        int bx = W / 2 - bw / 2, by = H / 2 - bh / 2;
        m_pCanvas->FillRect(bx, by, bw, bh, theme::BG);
        m_pCanvas->FillRect(bx, by, bw, 3, theme::ADJUST);
        m_pCanvas->Text(&g_font_ps2p8, 2, bx + 24, by + 22, "KEEP THIS MODE?",
                        theme::ADJUST, theme::BG, true);

        char line[40] = "Reverting in 00";
        line[13] = (char) ('0' + (sec / 10));
        line[14] = (char) ('0' + (sec % 10));
        m_pCanvas->Text(&g_font_vt323_22, 1, bx + 24, by + 70, line,
                        theme::WHITE, theme::BG, true);

        int hx = hint_button(m_pCanvas, bx + 24, by + 110, 'A', theme::ACTIVE, "KEEP");
        hint_button(m_pCanvas, hx, by + 110, 'B', theme::TEXT_DIM, "REVERT");

        // progress bar (drains with sec)
        int barW = bw - 48;
        m_pCanvas->FillRect(bx + 24, by + 150, barW, 8, theme::TEXT_DIM);
        m_pCanvas->FillRect(bx + 24, by + 150, barW * sec / 15, 8, theme::ADJUST);

        m_pCanvas->Scanlines(0, 0, W, H, 60);

        for (int t = 0; t < 62; t++)
        {
            m_pUSBHCI->UpdatePlugAndPlay();
            m_pGamepad->Poll();
            unsigned now = m_pGamepad->MenuButtons();
            unsigned pressed = now & ~prev;
            prev = now;
            if (pressed & GP_A) return TRUE;
            if (pressed & GP_B) return FALSE;
            CTimer::SimpleMsDelay(16);
        }
    }
    return FALSE;
}
```
Replace the "Mode unavailable" inline message in `Apply` with:
```cpp
    if (!m_pDisplay->SetMode(w, h))
    {
        int W = (int) m_pCanvas->Width();
        int H = (int) m_pCanvas->Height();
        int bw = 460, bh = 70, bx = W/2-bw/2, by = H/2-bh/2;
        m_pCanvas->FillRect(bx, by, bw, bh, theme::BG);
        m_pCanvas->FillRect(bx, by, bw, 3, theme::SELECTION);
        m_pCanvas->Text(&g_font_vt323_22, 1, bx + 24, by + 24,
                        "Mode unavailable.", theme::TEXT, theme::BG, true);
        CTimer::SimpleMsDelay(1200);
        return;
    }
```

- [ ] **Step 3: Flip the kernel constructor** (line ~55 → `&m_GlyphCanvas`).

- [ ] **Step 4: Build** — `make` → `kernel7.img`.

- [ ] **Step 5: Commit**
```bash
git add src/menu/video_mode_screen.h src/menu/video_mode_screen.cpp src/kernel.cpp
git commit -m "feat(menu): re-skin video mode (chip strip + confirm/revert + progress)"
```

---

### Task 7: Controls (07)

**Files:**
- Modify: `src/menu/controls_screen.h` (canvas type), `src/menu/controls_screen.cpp` (`Render`)
- Modify: `src/kernel.cpp` (constructor line 56)

- [ ] **Step 1: Update `controls_screen.h`** — swap to `GlyphCanvas`.

- [ ] **Step 2: Rewrite `Render` in `controls_screen.cpp`**

Add includes (`theme.h`, `screen_chrome.h`, `fonts/font_vt323_22.h`); remove `BOX/WHITE/SELFG/SELBG`. Replace `Render`:
```cpp
void ControlsScreen::Render(int player, int selected)
{
    using namespace chrome;
    const ButtonMap &map = (player == 0) ? m_pSettings->map1 : m_pSettings->map2;

    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "CONTROLS", player == 0 ? "Player 1" : "Player 2",
           theme::VALUE);

    // Row 0: player selector (adjustable). Rows 1..8: button maps (adjustable).
    value_row(m_pCanvas, 0, selected == 0, "Player",
              player == 0 ? "1" : "2", theme::VALUE, true);
    for (int b = 0; b < NUM_BTN; b++)
    {
        value_row(m_pCanvas, b + 1, selected == b + 1, GEN_LABEL[b],
                  phys_label(map.b[b]), theme::VALUE, true);
    }

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_dpad(m_pCanvas, PAD, fy, "NAVIGATE");
    hx = hint_lr(m_pCanvas, hx, fy, "REMAP");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
}
```
(The mock's controller diagram is omitted to keep the row layout clean and DRY with the other screens; the button rows carry the same information.)

- [ ] **Step 3: Flip the kernel constructor** (line ~56 → `&m_GlyphCanvas`).

- [ ] **Step 4: Build** — `make` → `kernel7.img`.

- [ ] **Step 5: Commit**
```bash
git add src/menu/controls_screen.h src/menu/controls_screen.cpp src/kernel.cpp
git commit -m "feat(menu): re-skin controls screen (player + button value rows)"
```

---

### Task 8: Hotkey + Calibration cluster

**Files:**
- Modify: `src/menu/hotkey_screen.h`/`.cpp`, `src/menu/calibration_screen.h`/`.cpp`
- Modify: `src/kernel.cpp` (constructor lines 57, 59)

These aren't in the mock but share the canvas; theme them consistently.

- [ ] **Step 1: Update both headers** — swap include + constructor param + member to `GlyphCanvas`.

- [ ] **Step 2: Rewrite `HotkeyScreen::Render`**

Add includes (`theme.h`, `screen_chrome.h`, `fonts/font_vt323_22.h`); remove `BOX/WHITE/RED/SELFG/SELBG`. Replace `Render`:
```cpp
void HotkeyScreen::Render(int selected)
{
    using namespace chrome;
    unsigned conflicts =
        hotkey_conflicts(m_pSettings->hotkeys, m_pSettings->menu_hotkey);

    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "HOTKEYS", "Select + button", theme::VALUE);

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  act   = i / 2;
        bool isKey = (i & 1);
        bool sel   = (i == selected);
        bool bad   = (conflicts >> act) & 1u;

        const HotkeyBinding &b = m_pSettings->hotkeys.b[act];
        char label[28]; int k = 0;
        const char *a = ACT_LABEL[act];
        for (int j = 0; a[j] && k < 18; j++) label[k++] = a[j];
        label[k++] = ' ';
        const char *f = isKey ? "key" : "mod";
        for (int j = 0; f[j]; j++) label[k++] = f[j];
        label[k] = '\0';

        const char *val = phys_label(isKey ? b.trigger : b.hold);
        int y = value_row(m_pCanvas, i, sel, label, val,
                          bad ? theme::SELECTION : theme::VALUE, true);
        if (bad) {
            int W = (int) m_pCanvas->Width();
            m_pCanvas->Text(&g_font_ps2p8, 1, W - PAD - 10, y + 4, "!",
                            theme::SELECTION, theme::BG, true);
        }
    }

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_dpad(m_pCanvas, PAD, fy, "NAVIGATE");
    hx = hint_lr(m_pCanvas, hx, fy, "CHANGE");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
}
```

- [ ] **Step 3: Rewrite `CalibrationScreen::Prompt` + `Message`**

Add includes (`theme.h`, `screen_chrome.h`, `fonts/font_ps2p8.h`, `fonts/font_vt323_22.h`); remove `BOX/WHITE`. Replace `Prompt`:
```cpp
void CalibrationScreen::Prompt(unsigned vid, unsigned pid, int idx)
{
    using namespace chrome;
    int W = (int) m_pCanvas->Width();
    m_pCanvas->Clear(theme::BG);

    char vp[12]; char vh[5], ph[5];
    hex4(vh, vid); hex4(ph, pid);
    int k = 0; for (int j = 0; vh[j]; j++) vp[k++] = vh[j];
    vp[k++] = ':'; for (int j = 0; ph[j]; j++) vp[k++] = ph[j]; vp[k] = '\0';
    header(m_pCanvas, "CALIBRATE", vp, theme::VALUE);

    char line[24] = "Press ";
    int n = 6; for (int j = 0; NAMES[idx][j] && n < 22; j++) line[n++] = NAMES[idx][j];
    line[n] = '\0';
    int tw = m_pCanvas->TextWidth(&g_font_ps2p8, 3, line);
    m_pCanvas->Text(&g_font_ps2p8, 3, W / 2 - tw / 2, 260, line,
                    theme::WHITE, theme::BG, true);

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_dpad(m_pCanvas, PAD, fy, "DOWN: SKIP");
    (void) hx;
    m_pCanvas->Text(&g_font_ps2p8, 1, PAD + 260, fy + 4, "LEFT: CANCEL",
                    theme::TEXT_DIM, theme::BG, true);
    scanlines(m_pCanvas);
}
```
Replace `Message`:
```cpp
void CalibrationScreen::Message(const char *text)
{
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();
    m_pCanvas->Clear(theme::BG);
    int tw = m_pCanvas->TextWidth(&g_font_ps2p8, 2, text);
    m_pCanvas->Text(&g_font_ps2p8, 2, W / 2 - tw / 2, H / 2 - 10, text,
                    theme::WHITE, theme::BG, true);
    m_pCanvas->Scanlines(0, 0, W, H, 60);
}
```

- [ ] **Step 4: Flip the kernel constructors** (lines ~57 hotkey, ~59 calibration → `&m_GlyphCanvas`).

- [ ] **Step 5: Build** — `make` → `kernel7.img`.

- [ ] **Step 6: Commit**
```bash
git add src/menu/hotkey_screen.h src/menu/hotkey_screen.cpp src/menu/calibration_screen.h src/menu/calibration_screen.cpp src/kernel.cpp
git commit -m "feat(menu): theme hotkey + calibration screens to match design"
```

---

### Task 9: Kernel error messages + final sweep + checklist

**Files:**
- Modify: `src/kernel.cpp` (the two ROM-error messages, lines ~229 and ~250)
- Modify: `docs/hardware-verification-checklist-2026-06-20.md`

- [ ] **Step 1: Theme the ROM error messages**

In `src/kernel.cpp`, replace each `m_Canvas.Clear(...)` + two `m_Canvas.DrawText(...)` error block (the "Failed to read ROM." and "Failed to load ROM." blocks) with `GlyphCanvas` equivalents, e.g.:
```cpp
			m_GlyphCanvas.Clear (0x0842 /*theme::BG*/);
			m_GlyphCanvas.Text (&g_font_ps2p8, 2, 40, 40,
			                    "FAILED TO READ ROM", 0xF800, 0x0842, true);
```
Add `#include "ui/fonts/font_ps2p8.h"` near the top of `src/kernel.cpp`. (Keep the second-line detail text using `g_font_vt323_22` if a second line is present; otherwise drop it.) The `m_Canvas` member stays (still used by `Overlay`).

- [ ] **Step 2: Full host test sweep**

Run: `make -C test run`
Expected: every line ends `OK`/`passed` (icons test now includes the `icon_tri` cases).

- [ ] **Step 3: Cross-build**

Run: `make`
Expected: `kernel7.img` built, no warnings about the migrated screens.

- [ ] **Step 4: Add hardware-verify items**

In `docs/hardware-verification-checklist-2026-06-20.md`, add a section `U. UI redesign rollout` with items: U1 splash (typographic), U2 pause menu (dim-behind panel + hints), U3 save-state slots (USED/EMPTY badges), U4 settings (rows + arrows + chevrons), U5 video mode (chip strip + confirm/revert + progress), U6 controls (player + button rows), U7 hotkeys (conflict `!` flag), U8 calibration prompt. Add a `| U. UI redesign rollout | | U1–U8 pending |` row to the results summary.

- [ ] **Step 5: Commit**
```bash
git add src/kernel.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "feat(kernel): theme ROM-error screens; docs: UI rollout verify items (U1-U8)"
```

---

## Self-Review

**Spec coverage** (design rollout screens):
- Splash 00 typographic → Task 3. ✓
- Pause 04 (dim-behind panel) → Task 4. ✓
- Save-state slots 08 (USED/EMPTY badges) → Task 4 (PickSlot). ✓
- Settings 05 (rows, arrows, chevrons) → Task 5. ✓
- Video mode 06 (chip strip + confirm/revert + progress) → Task 6. ✓
- Controls 07 (player + button rows) → Task 7. ✓
- Hotkey + Calibration (consistency, not in mock) → Task 8. ✓
- Shared chrome DRY → Task 2; adjust-arrow icon → Task 1. ✓
- Kernel error screens + verify log → Task 9. ✓
- Boot diagnostics 01 → intentionally deferred (decision). Controller diagram (07) → intentionally omitted (noted). `TextCanvas` retirement → deferred (Overlay still uses it).

**Placeholder scan:** No TBD/TODO; every step has concrete code. Manual digit formatting used throughout (no snprintf). ✓

**Type consistency:** `chrome::*` signatures defined in Task 2 are used as-is in Tasks 4–8. `value_row` returns `int y` (used by settings chevron and hotkey `!`). `icon_tri(...,dir,...)` defined Task 1, surfaced as `GlyphCanvas::IconTri` in Task 2, used by chrome + settings. Every migrated screen changes `.h` type, `.cpp` render, and its kernel constructor line in the same task, so the build stays green per task. `m_Canvas` is retained for `Overlay`; only the screen constructors flip to `m_GlyphCanvas`. ✓
