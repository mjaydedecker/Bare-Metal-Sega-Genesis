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

// Write-only pseudo-transparent fill: writes `color` on a 50% checkerboard and
// leaves every 3rd row (local row % 3 == 2) untouched as a scanline gap. Never
// READS the framebuffer (unlike blend_rect/scanlines), so it is cheap over the
// Pi's write-combining framebuffer where reads stall. Used for HUD/toast panels.
void gd_stipple_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                     int x, int y, int w, int h, uint16_t color);

int  gd_text_width(const Font *f, int scale, const char *s);

// Draws s with integer scale (>=1). When transparent, off pixels are left
// untouched (bg ignored). Returns pen x after the last glyph.
int  gd_draw_text (uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                   const Font *f, int scale, int x, int y, const char *s,
                   uint16_t fg, uint16_t bg, bool transparent);

#endif
