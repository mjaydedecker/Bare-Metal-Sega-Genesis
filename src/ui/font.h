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
