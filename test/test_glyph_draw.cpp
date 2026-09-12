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

    // scanlines at scale 2: 2-px dark bands on rows where (y / 2) % 3 == 2,
    // aligned to ABSOLUTE framebuffer rows (so region passes line up).
    reset();
    gd_fill_rect(fb, W, W, H, 0, 0, 8, 12, 0xFFFF);
    gd_scanlines(fb, W, W, H, 0, 0, 8, 12, 128, 2);
    assert(fb[3 * W + 0]  == 0xFFFF);         // row 3: (3/2)%3 = 1
    assert(fb[4 * W + 0]  != 0xFFFF);         // rows 4-5: band
    assert(fb[5 * W + 0]  != 0xFFFF);
    assert(fb[6 * W + 0]  == 0xFFFF);         // rows 6-9 untouched
    assert(fb[9 * W + 0]  == 0xFFFF);
    assert(fb[10 * W + 0] != 0xFFFF);         // rows 10-11: next band
    assert(fb[11 * W + 0] != 0xFFFF);

    // scale-2 region starting mid-band stays on the absolute row grid.
    reset();
    gd_fill_rect(fb, W, W, H, 0, 0, 8, 16, 0xFFFF);
    gd_scanlines(fb, W, W, H, 0, 5, 8, 6, 128, 2);
    assert(fb[4 * W + 0]  == 0xFFFF);         // outside region (would be band)
    assert(fb[5 * W + 0]  != 0xFFFF);         // (5/2)%3 == 2
    assert(fb[6 * W + 0]  == 0xFFFF);         // (6/2)%3 == 0
    assert(fb[10 * W + 0] != 0xFFFF);         // (10/2)%3 == 2

    // stipple at scale 2: 2x2 checker cells ((x/2 + y/2) & 1), gap rows where
    // ((y - y0) / 2) % 3 == 2. Still write-only.
    reset();
    gd_stipple_rect(fb, W, W, H, 0, 0, 8, 12, 0xABCD, 2);
    assert(fb[0 * W + 0] == 0xABCD);          // cell (0,0)
    assert(fb[0 * W + 1] == 0xABCD);          // same cell
    assert(fb[1 * W + 1] == 0xABCD);          // same cell
    assert(fb[0 * W + 2] == 0x0000);          // cell (1,0): odd
    assert(fb[2 * W + 2] == 0xABCD);          // cell (1,1): even
    assert(fb[4 * W + 0] == 0x0000);          // local rows 4-5: gap
    assert(fb[5 * W + 1] == 0x0000);
    assert(fb[6 * W + 2] == 0xABCD);          // row 6 resumes: cell (1,3) even
    assert(fb[6 * W + 0] == 0x0000);          // cell (0,3): odd

    // stipple scale-2 gap rows are LOCAL to the rect's y origin.
    reset();
    gd_stipple_rect(fb, W, W, H, 0, 3, 4, 8, 0x5555, 2);
    assert(fb[6 * W + 2] == 0x5555);          // local (6-3)/2 = 1; cell (1,3) even
    assert(fb[7 * W + 2] == 0x0000);          // local (7-3)/2 = 2 -> gap
    assert(fb[8 * W + 0] == 0x0000);          // local (8-3)/2 = 2 -> gap
    assert(fb[9 * W + 0] == 0x5555);          // local 3; cell (0,4) even

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
