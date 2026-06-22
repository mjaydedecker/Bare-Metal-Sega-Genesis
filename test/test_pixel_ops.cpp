#include "../src/ui/pixel_ops.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    // alpha extremes return the endpoints exactly
    assert(rgb565_blend(0x1234, 0xABCD, 0)   == 0x1234);
    assert(rgb565_blend(0x1234, 0xABCD, 255) == 0xABCD);

    // blending a color toward black at ~50% halves each channel (approx)
    // src = pure red 0xF800 (R=31), dst = black. alpha 128 -> R ~= 15..16
    uint16_t r = rgb565_blend(0x0000, 0xF800, 128);
    unsigned R = (r >> 11) & 0x1F;
    assert(R >= 15 && R <= 16);
    assert(((r >> 5) & 0x3F) == 0);
    assert((r & 0x1F) == 0);

    // blend is monotonic in alpha for a single channel
    uint16_t a = rgb565_blend(0x0000, 0x001F, 64);
    uint16_t b = rgb565_blend(0x0000, 0x001F, 192);
    assert((a & 0x1F) < (b & 0x1F));

    printf("test_pixel_ops OK\n");
    return 0;
}
