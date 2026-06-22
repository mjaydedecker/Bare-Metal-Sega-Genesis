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

    printf("test_icons OK\n");
    return 0;
}
