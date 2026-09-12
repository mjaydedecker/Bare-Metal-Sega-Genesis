#include "../src/ui/icons.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define W 24u
#define H 24u
static uint16_t fb[W * H];
static void reset(void){ memset(fb, 0, sizeof(fb)); }

// reuse the 3x5 '!' font from glyph_draw's test space
static const uint8_t kBitmap[] = {
    0x00,0x00,0x00,0x00,0x00,   // ' '
    0x40,0x40,0x40,0x40,0x40,   // '!'
};
static const Font kFont = { 0x20, 0x21, 3, 5, 1, kBitmap };

int main(void) {
    // play triangle: tip column near the right edge is set; far bottom-right is empty
    reset();
    icon_play(fb, W, W, H, 0, 0, 8, 0xF800);
    assert(fb[4 * W + 0] == 0xF800);          // left edge mid-height = base of triangle
    assert(fb[0 * W + 7] == 0x0000);          // top-right corner empty (tapered)

    // cross: center row and center column are set, corners empty
    reset();
    icon_cross(fb, W, W, H, 0, 0, 9, 0x07E0);
    assert(fb[4 * W + 4] == 0x07E0);          // center
    assert(fb[0 * W + 0] == 0x0000);          // corner empty

    // button: fill present, and at least one fg (letter) pixel inside
    reset();
    icon_button(fb, W, W, H, 0, 0, 11, '!', 0xE000, 0xFFFF, &kFont);
    assert(fb[0 * W + 0] == 0xE000);          // fill corner
    bool sawLetter = false;
    for (unsigned i = 0; i < W * H; i++) if (fb[i] == 0xFFFF) sawLetter = true;
    assert(sawLetter);
    // exact scale-1 placement: 3x5 glyph centred in 11 -> origin (4,3); the
    // '!' stroke is the glyph's middle column -> x = 5, rows 3..7.
    assert(fb[3 * W + 5] == 0xFFFF);
    assert(fb[7 * W + 5] == 0xFFFF);
    assert(fb[3 * W + 6] == 0xE000);
    assert(fb[8 * W + 5] == 0xE000);

    // button at scale 2: letter drawn 2x (6x10) and centred in d=12 ->
    // origin (3,1); stroke columns x = 5..6, rows 1..10.
    reset();
    icon_button(fb, W, W, H, 0, 0, 12, '!', 0xE000, 0xFFFF, &kFont, 2);
    assert(fb[1 * W + 5]  == 0xFFFF);
    assert(fb[1 * W + 6]  == 0xFFFF);
    assert(fb[10 * W + 6] == 0xFFFF);
    assert(fb[1 * W + 4]  == 0xE000);         // left of stroke
    assert(fb[1 * W + 7]  == 0xE000);         // right of stroke
    assert(fb[0 * W + 5]  == 0xE000);         // above letter
    assert(fb[11 * W + 5] == 0xE000);         // below letter

    // left triangle: base at right edge, tip at left-middle
    reset();
    icon_tri(fb, W, W, H, 0, 0, 8, 1, 0x07E0);
    assert(fb[4 * W + 7] == 0x07E0);          // right edge mid = base
    assert(fb[0 * W + 0] == 0x0000);          // top-left empty (tapered)

    // down triangle: base at top, tip at bottom-middle
    reset();
    icon_tri(fb, W, W, H, 0, 0, 8, 2, 0x07E0);
    assert(fb[0 * W + 4] == 0x07E0);          // top-middle = base
    assert(fb[7 * W + 0] == 0x0000);          // bottom-left empty

    printf("test_icons OK\n");
    return 0;
}
