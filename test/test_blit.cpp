#include "../src/video/blit.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define DST_W 320u
#define DST_H 240u
#define SRC_STRIDE_PX 720u   // core's fixed buffer width

static uint16_t dst[DST_W * DST_H];
static uint16_t src[SRC_STRIDE_PX * DST_H];

static void reset_buffers(void) {
    memset(dst, 0, sizeof(dst));
    memset(src, 0, sizeof(src));
}

// scale=1: centering 256x224 into 320x240 -> offX=32, offY=8.
static void test_centering_256x224(void) {
    reset_buffers();
    for (unsigned y = 0; y < 224; y++)
        src[y * SRC_STRIDE_PX + 0] = (uint16_t)(y + 1);
    src[223 * SRC_STRIDE_PX + 255] = 0xBEEF; // last visible pixel

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 256, 224, 1);

    unsigned offX = (DST_W - 256) / 2; // 32
    unsigned offY = (DST_H - 224) / 2; // 8
    assert(dst[(offY + 0) * DST_W + offX] == 1);
    assert(dst[(offY + 10) * DST_W + offX] == 11);
    assert(dst[(offY + 223) * DST_W + (offX + 255)] == 0xBEEF);
    assert(dst[0] == 0);
    printf("test_centering_256x224 OK\n");
}

// Source stride must be honored (720, not width=256).
static void test_source_stride_honored(void) {
    reset_buffers();
    for (unsigned y = 0; y < 224; y++)
        src[y * SRC_STRIDE_PX] = (uint16_t)(0x100 + y);

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 256, 224, 1);

    unsigned offX = 32, offY = 8;
    assert(dst[(offY + 1) * DST_W + offX] == 0x101);
    assert(dst[(offY + 5) * DST_W + offX] == 0x105);
    printf("test_source_stride_honored OK\n");
}

// scale=1 full-width 320x240 -> no offset.
static void test_full_320x240(void) {
    reset_buffers();
    src[0] = 0xAAAA;
    src[239 * SRC_STRIDE_PX + 319] = 0x5555;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 320, 240, 1);

    assert(dst[0] == 0xAAAA);
    assert(dst[239 * DST_W + 319] == 0x5555);
    printf("test_full_320x240 OK\n");
}

// Oversized input is clamped (no overflow, no crash).
static void test_clamp_oversized(void) {
    reset_buffers();
    for (unsigned i = 0; i < SRC_STRIDE_PX * DST_H; i++) src[i] = 0x1234;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 400, 300, 1); // exceeds surface

    assert(dst[0] == 0x1234);
    assert(dst[DST_W * DST_H - 1] == 0x1234);
    printf("test_clamp_oversized OK\n");
}

// NULL source is a no-op (dupe frame).
static void test_null_src_noop(void) {
    reset_buffers();
    for (unsigned i = 0; i < DST_W * DST_H; i++) dst[i] = 0x4321;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                NULL, SRC_STRIDE_PX * 2, 256, 224, 1);

    assert(dst[0] == 0x4321);
    assert(dst[DST_W * DST_H - 1] == 0x4321);
    printf("test_null_src_noop OK\n");
}

// scale=0 is a no-op (invalid).
static void test_scale_zero_noop(void) {
    reset_buffers();
    for (unsigned i = 0; i < DST_W * DST_H; i++) dst[i] = 0x7777;
    src[0] = 0x1111;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 256, 224, 0);

    assert(dst[0] == 0x7777);
    printf("test_scale_zero_noop OK\n");
}

// scale=2: each source pixel becomes a 2x2 block, centered.
static void test_scale_2x_block(void) {
    reset_buffers();
    // 100x50 source scaled 2x -> 200x100 output.
    src[0] = 0x1111;                       // top-left source pixel
    src[1 * SRC_STRIDE_PX + 1] = 0x2222;   // source (1,1)

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 100, 50, 2);

    unsigned offX = (DST_W - 200) / 2; // 60
    unsigned offY = (DST_H - 100) / 2; // 70
    // top-left source pixel replicated into a 2x2 block
    assert(dst[(offY + 0) * DST_W + (offX + 0)] == 0x1111);
    assert(dst[(offY + 0) * DST_W + (offX + 1)] == 0x1111);
    assert(dst[(offY + 1) * DST_W + (offX + 0)] == 0x1111);
    assert(dst[(offY + 1) * DST_W + (offX + 1)] == 0x1111);
    // source (1,1) lands at dst block starting (offX+2, offY+2)
    assert(dst[(offY + 2) * DST_W + (offX + 2)] == 0x2222);
    assert(dst[(offY + 3) * DST_W + (offX + 3)] == 0x2222);
    printf("test_scale_2x_block OK\n");
}

// scale too large for the surface is reduced to fit.
static void test_scale_reduced_to_fit(void) {
    reset_buffers();
    src[0] = 0x9999;
    // 320x240 at requested scale 10 cannot fit; must reduce to 1.
    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 320, 240, 10);

    assert(dst[0] == 0x9999);            // scale fell back to 1, offset 0
    printf("test_scale_reduced_to_fit OK\n");
}

// blit_rgb565_scaled: 2x1 source stretched to a 4x2 rect, centered in 320x240.
static void test_scaled_stretch_rect(void) {
    reset_buffers();
    src[0] = 0xAAAA;                 // source (0,0)
    src[1] = 0xBBBB;                 // source (1,0)

    // Stretch a 2x1 image into a 4x2 rect at offset (10, 20).
    blit_rgb565_scaled(dst, DST_W * 2, DST_W, DST_H,
                       src, SRC_STRIDE_PX * 2, 2, 1,
                       10, 20, 4, 2);

    // Columns 0..1 map to source col 0; columns 2..3 map to source col 1.
    assert(dst[20 * DST_W + 10] == 0xAAAA);
    assert(dst[20 * DST_W + 11] == 0xAAAA);
    assert(dst[20 * DST_W + 12] == 0xBBBB);
    assert(dst[20 * DST_W + 13] == 0xBBBB);
    // Row 1 replicates row 0 (single source row).
    assert(dst[21 * DST_W + 10] == 0xAAAA);
    assert(dst[21 * DST_W + 13] == 0xBBBB);
    // Outside the rect stays clear.
    assert(dst[20 * DST_W + 9] == 0);
    printf("test_scaled_stretch_rect OK\n");
}

// blit_rgb565_scaled: rect that leaves the surface is a no-op.
static void test_scaled_out_of_bounds_noop(void) {
    reset_buffers();
    src[0] = 0x1234;
    blit_rgb565_scaled(dst, DST_W * 2, DST_W, DST_H,
                       src, SRC_STRIDE_PX * 2, 2, 2,
                       DST_W - 1, 0, 4, 4);   // exceeds width
    assert(dst[0] == 0);
    printf("test_scaled_out_of_bounds_noop OK\n");
}

// 320x224 and 256x224 into 1920x1080 must produce the SAME width and a 4:3 box.
static void test_aspect_h32_h40_same_width(void) {
    unsigned x40, y40, w40, h40, x32, y32, w32, h32;
    aspect_rect(1920, 1080, 320, 224, &x40, &y40, &w40, &h40);
    aspect_rect(1920, 1080, 256, 224, &x32, &y32, &w32, &h32);

    // Same source height -> identical output box for both widths.
    assert(w40 == w32);
    assert(h40 == h32);
    // Vertical scale is integer: out_h is a whole multiple of source height.
    assert(h40 % 224 == 0);
    // Box is ~4:3 (within integer rounding of the /3 *4).
    assert(w40 * 3 <= h40 * 4 && (w40 + 4) * 3 >= h40 * 4);
    // Fits and is centered.
    assert(x40 + w40 <= 1920 && y40 + h40 <= 1080);
    assert(x40 == (1920 - w40) / 2 && y40 == (1080 - h40) / 2);
    printf("test_aspect_h32_h40_same_width OK\n");
}

// Vertical integer scale picks the largest whole factor fitting the 4:3 box.
static void test_aspect_vertical_integer(void) {
    unsigned x, y, w, h;
    aspect_rect(1920, 1080, 320, 224, &x, &y, &w, &h);
    // 4:3 box in 1920x1080 is 1440x1080; 1080/224 = 4 -> out_h = 896.
    assert(h == 896);
    assert(w == 896 * 4 / 3);  // 1194
    printf("test_aspect_vertical_integer OK\n");
}

// Degenerate inputs are safe (all zeros).
static void test_aspect_degenerate(void) {
    unsigned x = 9, y = 9, w = 9, h = 9;
    aspect_rect(1920, 1080, 0, 224, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);
    aspect_rect(0, 0, 320, 224, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);
    printf("test_aspect_degenerate OK\n");
}

int main(void) {
    test_centering_256x224();
    test_source_stride_honored();
    test_full_320x240();
    test_clamp_oversized();
    test_null_src_noop();
    test_scale_zero_noop();
    test_scale_2x_block();
    test_scale_reduced_to_fit();
    test_scaled_stretch_rect();
    test_scaled_out_of_bounds_noop();
    test_aspect_h32_h40_same_width();
    test_aspect_vertical_integer();
    test_aspect_degenerate();
    printf("All blit tests passed\n");
    return 0;
}
