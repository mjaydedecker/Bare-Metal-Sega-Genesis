#include "../src/ui/glyph_draw.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define W 32u
#define H 16u
static uint16_t fb[W * H];

// 3x5 font holding two glyphs: '!' (full column) at code 0x21 and ' ' (blank)
// at 0x20. stride=1. Rows are MSB-first; we use the top 3 bits.
//  ' ' : all rows 0
//  '!' : a vertical bar in the middle column -> 0b010xxxxx = 0x40
static const uint8_t kBitmap[] = {
    0x00,0x00,0x00,0x00,0x00,   // 0x20 ' '
    0x40,0x40,0x40,0x40,0x40,   // 0x21 '!'
};
static const Font kFont = { 0x20, 0x21, 3, 5, 1, kBitmap };

static void reset(void){ memset(fb, 0, sizeof(fb)); }

int main(void) {
    // fill_rect writes a solid block, clipped to the buffer
    reset();
    gd_fill_rect(fb, W, W, H, 2, 3, 4, 2, 0xBEEF);
    assert(fb[3 * W + 2] == 0xBEEF);
    assert(fb[4 * W + 5] == 0xBEEF);
    assert(fb[3 * W + 6] == 0x0000);   // just outside the width
    // negative origin is clipped, no crash, no out-of-bounds write
    gd_fill_rect(fb, W, W, H, -4, -4, 6, 6, 0x1111);
    assert(fb[0] == 0x1111);

    // blend_rect at alpha 255 == solid
    reset();
    gd_blend_rect(fb, W, W, H, 0, 0, 3, 3, 0x07E0, 255);
    assert(fb[0] == 0x07E0);

    // scanlines darken every 3rd row (y % 3 == 2) within the region only
    reset();
    gd_fill_rect(fb, W, W, H, 0, 0, 8, 6, 0xFFFF);
    gd_scanlines(fb, W, W, H, 0, 0, 8, 6, 128);
    assert(fb[0 * W + 0] == 0xFFFF);          // row 0 untouched
    assert(fb[2 * W + 0] != 0xFFFF);          // row 2 darkened
    assert(fb[2 * W + 0] != 0x0000);          // but not fully black

    // stipple_rect: write-only 50% checkerboard, every 3rd local row skipped.
    reset();
    gd_stipple_rect(fb, W, W, H, 0, 0, 8, 6, 0xABCD);
    assert(fb[0 * W + 0] == 0xABCD);          // (0,0) even -> color
    assert(fb[0 * W + 1] == 0x0000);          // (1,0) odd  -> untouched (game shows)
    assert(fb[1 * W + 1] == 0xABCD);          // (1,1) even -> color
    assert(fb[1 * W + 0] == 0x0000);          // (0,1) odd  -> untouched
    assert(fb[2 * W + 0] == 0x0000);          // local row 2 = scanline gap, untouched
    assert(fb[2 * W + 2] == 0x0000);
    assert(fb[3 * W + 1] == 0xABCD);          // row 3 resumes checkerboard
    assert(fb[0 * W + 8] == 0x0000);          // outside width, untouched
    gd_stipple_rect(fb, W, W, H, -3, -3, 6, 6, 0x2222);   // negative origin clips, no crash

    // text width = chars * width * scale
    assert(gd_text_width(&kFont, 1, "!!") == 6);
    assert(gd_text_width(&kFont, 2, "!")  == 6);

    // draw '!' at scale 1: middle column set to fg, others bg
    reset();
    int penx = gd_draw_text(fb, W, W, H, &kFont, 1, 0, 0, "!", 0xF800, 0x001F, false);
    assert(penx == 3);
    assert(fb[0 * W + 1] == 0xF800);          // middle column, glyph on
    assert(fb[0 * W + 0] == 0x001F);          // left column, bg drawn
    // transparent mode leaves bg pixels as they were
    reset();
    fb[0 * W + 0] = 0x1234;
    gd_draw_text(fb, W, W, H, &kFont, 1, 0, 0, "!", 0xF800, 0x001F, true);
    assert(fb[0 * W + 0] == 0x1234);          // untouched (glyph off here)
    assert(fb[0 * W + 1] == 0xF800);          // glyph on

    // scale 2 makes each on-pixel a 2x2 block
    reset();
    gd_draw_text(fb, W, W, H, &kFont, 2, 0, 0, "!", 0xF800, 0x0000, false);
    assert(fb[0 * W + 2] == 0xF800 && fb[1 * W + 3] == 0xF800);

    printf("test_glyph_draw OK\n");
    return 0;
}
