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
