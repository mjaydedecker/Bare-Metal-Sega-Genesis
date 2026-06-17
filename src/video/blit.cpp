//
// src/video/blit.cpp
//
// Bare Metal Sega Genesis
// Pure RGB565 centered blit. See blit.h.
//

#include "blit.h"
#include <string.h>

void blit_rgb565(uint16_t *dst, unsigned dst_pitch_bytes,
                 unsigned dst_w, unsigned dst_h,
                 const uint16_t *src, unsigned src_pitch_bytes,
                 unsigned w, unsigned h)
{
    if (dst == 0 || src == 0) return;          // dupe frame / no surface
    if (w > dst_w) w = dst_w;                   // clamp defensively
    if (h > dst_h) h = dst_h;

    unsigned off_x      = (dst_w - w) / 2;
    unsigned off_y      = (dst_h - h) / 2;
    unsigned dst_stride = dst_pitch_bytes / 2;  // in pixels
    unsigned src_stride = src_pitch_bytes / 2;

    for (unsigned y = 0; y < h; y++)
    {
        memcpy(dst + (off_y + y) * dst_stride + off_x,
               src + y * src_stride,
               (size_t) w * 2);
    }
}
