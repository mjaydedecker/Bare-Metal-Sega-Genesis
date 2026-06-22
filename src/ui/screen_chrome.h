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
