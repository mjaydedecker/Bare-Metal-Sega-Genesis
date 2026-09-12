# UI Scaling by Output Resolution — Design

**Date:** 2026-09-11
**Status:** Approved design (pending implementation plan)

## Problem

All on-screen UI (ROM browser, pause menu, Settings and its sub-screens, the
in-game HUD/toasts) draws with fixed pixel sizes. The v1.0.0 redesign was laid
out for the 1080p framebuffer, but on a TV the 8 px Press Start 2P and 22 px
VT323 text is hard to read at 1080p. The framebuffer is the HDMI output mode
(see M5 video notes), so higher modes make the UI physically smaller.

## Goal

Menus grow with the output resolution in **whole-number steps** so bitmap fonts
stay pixel-perfect:

| Output        | Scale | Logical UI space |
|---------------|-------|------------------|
| 480p (720×480)   | 1× | 720×480   |
| 720p (1280×720)  | 1× | 1280×720  |
| 1080p (1920×1080)| 2× | 960×540   |
| 1440p (2560×1440)| 3× | 853×480   |

Non-goals: fractional scaling; changing screen layouts; changing the game image
(`Display::Blit`) or the image splash (`TextCanvas`, `SD:/splash.raw`).

## Approach

Scale inside `GlyphCanvas` (chosen over per-screen scaling and over rendering
to an off-screen buffer and upscaling). Screens keep drawing in their existing
coordinates; the canvas presents a smaller logical surface and multiplies on
the way to the framebuffer. One change point, no screen-code edits, and every
GlyphCanvas user scales identically.

## 1. Scale rule

New pure module `src/ui/ui_scale.{h,cpp}`:

```
unsigned ui_scale(unsigned fb_w, unsigned fb_h);
// = max(1, min(fb_h / 480, fb_w / 640))
```

- Guarantees a logical surface of at least 640×480, which every existing
  screen already fits (all screens are verified at 480p = 720×480; layouts need
  ≥640 wide, ≥~472 tall for the Controller Test screen).
- The width term keeps non-16:9 native modes safe (1280×1024 → 2 → 640×512;
  1024×768 → 1).
- Integer division; never returns 0.

## 2. GlyphCanvas contract

- Scale is **recomputed on every call** from the Display's current physical
  size (not cached), so live `Display::SetMode` changes — including the video
  mode confirm-or-revert dialog — rescale immediately.
- `Width()` / `Height()` return **logical** size: physical / scale.
- Every draw maps logical → physical: `x*s, y*s, w*s, h*s`; `Text` draws at
  `fontScale * s`; icon sizes `* s`.
- `TextWidth()` stays **logical** (`gd_text_width(f, fontScale, str)`), so
  existing centring math (`W/2 - tw/2`) is unchanged.
- `Clear()` fills the **full physical** framebuffer (logical size × scale can be
  up to s−1 px short, e.g. 2560/3).
- Clipping remains physical inside the pure primitives.

Nothing outside `GlyphCanvas` mixes logical and physical coordinates: menus
only use `GlyphCanvas::Width/Height`; the typographic splash
(`splash_show_text`) lays out from the canvas; `draw_image` uses Display
physical dims with `TextCanvas` and is unaffected.

**In scope (scales automatically):** ROM browser, pause menu + slot picker +
messages, Settings and all sub-screens (Controls, Hotkeys, Video Mode,
Calibrate, Controller Test), kernel "failed to read/load ROM" screens,
typographic boot splash, HUD and toasts.

## 3. Primitive changes (pure, host-tested)

Each gains a trailing `scale` parameter **defaulting to 1**; scale 1 is
pixel-identical to today, so existing callers and tests are unaffected.

- `gd_scanlines(..., uint8_t strength, int scale = 1)`: darken rows where
  `(yy / scale) % 3 == 2`. Stays aligned to absolute framebuffer rows, so the
  full-screen pass and the pause menu's panel-limited pass still register.
- `gd_stipple_rect(..., uint16_t color, int scale = 1)`: checkerboard on
  `((xx / scale) + (yy / scale)) & 1`; scanline-gap rows where
  `((yy - y) / scale) % 3 == 2`. Still write-only (no framebuffer reads — the
  HUD's performance constraint).
- `icon_button(..., const Font *f, int scale = 1)`: letter drawn at `scale`,
  centred using `gd_text_width(f, scale, …)` and `f->height * scale`.

Unchanged: `gd_fill_rect`, `gd_blend_rect`, `gd_draw_text` (canvas passes the
multiplied rect/position/scale); `icon_tri`, `icon_play`, `icon_cross` (drawn
per row at their size, so a scaled size is smooth, not blocky).

## 4. HUD / toasts

`Overlay` keeps drawing through the same canvas. Its own `hud_scale()` becomes
redundant now that `GlyphCanvas` owns resolution scaling — a second
HUD-specific scale on top of it would double-scale the HUD — so `hud_scale()`
is removed together with `HUD_SCALE_BASE` and its asserts in `test_hud.cpp`;
the `* sc` factors in `overlay.cpp` are dropped.

Net effect: ≤720p unchanged; 1080p stays 2× (same physical pixels and per-frame
cost as today); 1440p becomes 3× (was 2×), matching the menus.

## 5. Testing

Host tests (written first):
- `test_ui_scale` (new): 720×480→1, 1280×720→1, 1920×1080→2, 2560×1440→3,
  3840×2160→4, 1280×1024→2, 1024×768→1, 1024×1024→1 (width-limited: height
  alone would give 2), 0×0→1, 320×240→1.
- `test_glyph_draw`: `gd_scanlines` and `gd_stipple_rect` at scale 2 produce
  2-px bands / 2×2 checker cells; scale 1 output unchanged.
- `test_icons`: `icon_button` at scale 2 draws a centred 2× letter; scale 1
  unchanged.
- `test_hud`: remove `hud_scale` asserts.

`GlyphCanvas` is Circle-bound (Display) and not host-tested; it is thin
multiply-through code covered by the build and the hardware checklist.

Build: Pi 2 (`RASPPI=2`), warning-free, all libs/objects ARMv7 (readelf check).

## 6. Hardware checklist Y (new doc)

`docs/hardware-checklist-ui-scaling.md`:
- Y1. 480p and 720p: every screen looks exactly as before.
- Y2. 1080p: ROM browser at 2× (~13 rows), scrolls correctly.
- Y3. 1080p: pause menu 2× with dim + scanlines; selector moves don't lighten
  the background.
- Y4. 1080p: Settings and every sub-screen readable and fully on screen,
  including Controller Test (four panels + footer).
- Y5. Live switch 720p → 1080p in Video Mode rescales at once; confirm-or-revert
  dialog readable at the new size; revert restores 1×.
- Y6. 1080p: HUD and toasts the same size as before; gameplay FPS with HUD on
  unchanged.
- Y7. Native mode on a 1080p TV behaves as 1080p (2×).
- Y8. Typographic boot splash centred and readable at 1080p.

## 7. Risks

- Fewer visible list rows at 1080p (inherent to larger text).
- Any code treating `GlyphCanvas::Width()` as physical would be off by the
  scale — audited: none outside `GlyphCanvas`.
- 2× glyph drawing writes 4× the pixels per glyph in menus (not in gameplay,
  where HUD cost is unchanged at 1080p).

## 8. Rollout

Branch `feat/ui-scaling` from `main` (independent of unmerged
`feat/make-dist`); merge after host tests and the Pi 2 build pass; hardware
verification via checklist Y.
