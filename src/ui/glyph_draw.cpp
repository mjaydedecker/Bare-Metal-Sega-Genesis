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

void gd_stipple_rect(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                     int x, int y, int w, int h, uint16_t color) {
    for (int yy = y; yy < y + h; yy++) {
        if ((yy - y) % 3 == 2) continue;        // scanline gap row (write nothing)
        for (int xx = x; xx < x + w; xx++) {
            if (((xx + yy) & 1) != 0) continue;  // 50% checkerboard (write-only)
            put(buf, pitchPx, fbw, fbh, xx, yy, color);
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
