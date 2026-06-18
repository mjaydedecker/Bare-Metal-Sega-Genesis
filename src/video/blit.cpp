//
// src/video/blit.cpp
//
// Bare Metal Sega Genesis
// Pure RGB565 centered blit. See blit.h.
//

#include "blit.h"
#include <string.h>

// Max scaled row width we can stage in the line buffer (covers up to 4K wide).
#define BLIT_MAX_LINE 4096

void blit_rgb565(uint16_t *dst, unsigned dst_pitch_bytes,
                 unsigned dst_w, unsigned dst_h,
                 const uint16_t *src, unsigned src_pitch_bytes,
                 unsigned w, unsigned h, unsigned scale)
{
    if (dst == 0 || src == 0 || scale == 0) return;  // dupe frame / no surface

    // Reduce the scale factor until the scaled image fits the surface.
    while (scale > 1 && (w * scale > dst_w || h * scale > dst_h))
    {
        scale--;
    }

    // Clamp source extent so the scaled image fits at the (reduced) scale.
    if (w * scale > dst_w) w = dst_w / scale;
    if (h * scale > dst_h) h = dst_h / scale;

    unsigned out_w      = w * scale;
    unsigned out_h      = h * scale;
    if (out_w > BLIT_MAX_LINE) return;               // safety bound
    unsigned off_x      = (dst_w - out_w) / 2;
    unsigned off_y      = (dst_h - out_h) / 2;
    unsigned dst_stride = dst_pitch_bytes / 2;       // in pixels
    unsigned src_stride = src_pitch_bytes / 2;

    // Stage each horizontally-scaled row once in a cached line buffer, then
    // burst-copy it to the `scale` destination rows. This avoids ~scale^2
    // individual uncached framebuffer stores per source pixel.
    static uint16_t line[BLIT_MAX_LINE];
    const size_t    rowBytes = (size_t) out_w * 2;

    for (unsigned sy = 0; sy < h; sy++)
    {
        const uint16_t *srow = src + sy * src_stride;

        // Build one horizontally-scaled row (cached writes).
        uint16_t *lp = line;
        for (unsigned sx = 0; sx < w; sx++)
        {
            uint16_t px = srow[sx];
            for (unsigned dx = 0; dx < scale; dx++)
            {
                *lp++ = px;
            }
        }

        // Replicate it down `scale` framebuffer rows (burst copies).
        uint16_t *drow = dst + (off_y + sy * scale) * dst_stride + off_x;
        for (unsigned dy = 0; dy < scale; dy++)
        {
            memcpy(drow, line, rowBytes);
            drow += dst_stride;
        }
    }
}
