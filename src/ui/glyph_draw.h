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
