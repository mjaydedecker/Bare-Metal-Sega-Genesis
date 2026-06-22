//
// src/ui/pixel_ops.cpp
//
// Bare Metal Sega Genesis
// See pixel_ops.h.
//
#include "pixel_ops.h"

uint16_t rgb565_blend(uint16_t dst, uint16_t src, uint8_t alpha) {
    unsigned a = alpha;
    unsigned ia = 255u - a;

    unsigned dr = (dst >> 11) & 0x1F, dg = (dst >> 5) & 0x3F, db = dst & 0x1F;
    unsigned sr = (src >> 11) & 0x1F, sg = (src >> 5) & 0x3F, sb = src & 0x1F;

    unsigned r = (sr * a + dr * ia + 127) / 255;
    unsigned g = (sg * a + dg * ia + 127) / 255;
    unsigned b = (sb * a + db * ia + 127) / 255;

    return (uint16_t) ((r << 11) | (g << 5) | b);
}
