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
