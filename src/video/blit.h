//
// src/video/blit.h
//
// Bare Metal Sega Genesis
// Pure RGB565 centered blit — no Circle dependencies, host-testable.
//

#ifndef _video_blit_h
#define _video_blit_h

#include <stdint.h>
#include <stddef.h>

// Copy a w*h RGB565 image, nearest-neighbor integer-scaled by `scale` and
// centered, into a dst_w*dst_h RGB565 surface. Each source pixel becomes a
// scale*scale block. Strides are in BYTES; src may have a stride wider than w
// (the core's buffer pitch is fixed at 720*2 while the active width varies).
//
// src == NULL or scale == 0 is a no-op (libretro "repeat last frame").
// If the scaled image would not fit, `scale` is reduced until it does (min 1),
// then w/h are clamped to the destination. The caller is responsible for
// clearing letterbox/pillarbox bars.
void blit_rgb565(uint16_t *dst, unsigned dst_pitch_bytes,
                 unsigned dst_w, unsigned dst_h,
                 const uint16_t *src, unsigned src_pitch_bytes,
                 unsigned w, unsigned h, unsigned scale);

// Nearest-neighbor scale a w*h RGB565 image into the destination rectangle
// (out_x, out_y, out_w, out_h) inside a dst_w*dst_h RGB565 surface. Used by the
// "stretch" video mode for non-integer aspect-fill scaling. Strides are in
// BYTES. No-op if src is NULL, any extent is 0, or the rect leaves the surface.
// The caller clears letterbox/pillarbox bars.
void blit_rgb565_scaled(uint16_t *dst, unsigned dst_pitch_bytes,
                        unsigned dst_w, unsigned dst_h,
                        const uint16_t *src, unsigned src_pitch_bytes,
                        unsigned w, unsigned h,
                        unsigned out_x, unsigned out_y,
                        unsigned out_w, unsigned out_h);

#endif
