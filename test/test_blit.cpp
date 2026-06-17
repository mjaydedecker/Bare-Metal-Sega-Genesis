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

// Centering: 256x224 into 320x240 -> offX=32, offY=8.
static void test_centering_256x224(void) {
    reset_buffers();
    // Mark each source row's first pixel with a unique value.
    for (unsigned y = 0; y < 224; y++)
        src[y * SRC_STRIDE_PX + 0] = (uint16_t)(y + 1);
    src[223 * SRC_STRIDE_PX + 255] = 0xBEEF; // last visible pixel

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 256, 224);

    unsigned offX = (DST_W - 256) / 2; // 32
    unsigned offY = (DST_H - 224) / 2; // 8
    assert(dst[(offY + 0) * DST_W + offX] == 1);
    assert(dst[(offY + 10) * DST_W + offX] == 11);
    assert(dst[(offY + 223) * DST_W + (offX + 255)] == 0xBEEF);
    // Outside the centered region stays untouched (caller clears bars).
    assert(dst[0] == 0);
    printf("test_centering_256x224 OK\n");
}

// Source stride must be honored (720, not width=256).
static void test_source_stride_honored(void) {
    reset_buffers();
    for (unsigned y = 0; y < 224; y++)
        src[y * SRC_STRIDE_PX] = (uint16_t)(0x100 + y);

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 256, 224);

    unsigned offX = 32, offY = 8;
    // Row 1's marker would be wrong if stride were treated as 256.
    assert(dst[(offY + 1) * DST_W + offX] == 0x101);
    assert(dst[(offY + 5) * DST_W + offX] == 0x105);
    printf("test_source_stride_honored OK\n");
}

// Full-width 320x240 -> no offset.
static void test_full_320x240(void) {
    reset_buffers();
    src[0] = 0xAAAA;
    src[239 * SRC_STRIDE_PX + 319] = 0x5555;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 320, 240);

    assert(dst[0] == 0xAAAA);
    assert(dst[239 * DST_W + 319] == 0x5555);
    printf("test_full_320x240 OK\n");
}

// Oversized input is clamped (no overflow, no crash).
static void test_clamp_oversized(void) {
    reset_buffers();
    for (unsigned i = 0; i < SRC_STRIDE_PX * DST_H; i++) src[i] = 0x1234;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                src, SRC_STRIDE_PX * 2, 400, 300); // exceeds surface

    assert(dst[0] == 0x1234);
    assert(dst[DST_W * DST_H - 1] == 0x1234);
    printf("test_clamp_oversized OK\n");
}

// NULL source is a no-op (dupe frame).
static void test_null_src_noop(void) {
    reset_buffers();
    for (unsigned i = 0; i < DST_W * DST_H; i++) dst[i] = 0x4321;

    blit_rgb565(dst, DST_W * 2, DST_W, DST_H,
                NULL, SRC_STRIDE_PX * 2, 256, 224);

    assert(dst[0] == 0x4321);
    assert(dst[DST_W * DST_H - 1] == 0x4321);
    printf("test_null_src_noop OK\n");
}

int main(void) {
    test_centering_256x224();
    test_source_stride_honored();
    test_full_320x240();
    test_clamp_oversized();
    test_null_src_noop();
    printf("All blit tests passed\n");
    return 0;
}
