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

void blit_rgb565_scaled(uint16_t *dst, unsigned dst_pitch_bytes,
                        unsigned dst_w, unsigned dst_h,
                        const uint16_t *src, unsigned src_pitch_bytes,
                        unsigned w, unsigned h,
                        unsigned out_x, unsigned out_y,
                        unsigned out_w, unsigned out_h)
{
    if (dst == 0 || src == 0) return;
    if (w == 0 || h == 0 || out_w == 0 || out_h == 0) return;
    if (out_x + out_w > dst_w || out_y + out_h > dst_h) return;  // doesn't fit

    unsigned dst_stride = dst_pitch_bytes / 2;   // pixels
    unsigned src_stride = src_pitch_bytes / 2;

    for (unsigned dy = 0; dy < out_h; dy++)
    {
        unsigned        sy   = dy * h / out_h;   // nearest-neighbor source row
        const uint16_t *srow = src + sy * src_stride;
        uint16_t       *drow = dst + (out_y + dy) * dst_stride + out_x;
        for (unsigned dx = 0; dx < out_w; dx++)
        {
            drow[dx] = srow[dx * w / out_w];
        }
    }
}

void aspect_rect(unsigned fb_w, unsigned fb_h, unsigned w, unsigned h,
                 unsigned *out_x, unsigned *out_y,
                 unsigned *out_w, unsigned *out_h)
{
    *out_x = *out_y = *out_w = *out_h = 0;
    if (w == 0 || h == 0 || fb_w == 0 || fb_h == 0) return;

    // Largest 4:3 rectangle that fits the framebuffer.
    unsigned rh = fb_w * 3 / 4;
    if (rh > fb_h) rh = fb_h;     // height-bound; rw would be rh*4/3 (implicit)

    // Vertical: largest integer factor that fits the 4:3 box height.
    unsigned sv = rh / h;
    if (sv < 1) sv = 1;
    unsigned oh = h * sv;

    // Horizontal: stretch to 4:3 width, clamped to the framebuffer.
    unsigned ow = oh * 4 / 3;
    if (ow > fb_w) ow = fb_w;

    *out_w = ow;
    *out_h = oh;
    *out_x = (fb_w - ow) / 2;
    *out_y = (fb_h - oh) / 2;
}
