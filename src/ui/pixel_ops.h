//
// src/ui/pixel_ops.h
//
// Bare Metal Sega Genesis
// Pure RGB565 pixel math (host-testable, no Circle deps).
//
#ifndef _ui_pixel_ops_h
#define _ui_pixel_ops_h
#include <stdint.h>

// Blend src over dst in RGB565. alpha: 0 => dst unchanged, 255 => src fully.
uint16_t rgb565_blend(uint16_t dst, uint16_t src, uint8_t alpha);

#endif
