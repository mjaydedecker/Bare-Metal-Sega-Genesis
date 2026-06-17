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

// Copy a w*h RGB565 image, centered, into a dst_w*dst_h RGB565 surface.
// Strides are in BYTES. src may have a stride wider than w (the core's
// buffer pitch is fixed at 720*2 while the active width varies).
// src == NULL is a no-op (libretro "repeat last frame"). w/h are clamped
// to the destination. Caller is responsible for clearing letterbox bars.
void blit_rgb565(uint16_t *dst, unsigned dst_pitch_bytes,
                 unsigned dst_w, unsigned dst_h,
                 const uint16_t *src, unsigned src_pitch_bytes,
                 unsigned w, unsigned h);

#endif
