#include "../src/ui/fonts/font_ps2p8.h"
#include "../src/ui/fonts/font_vt323_16.h"
#include "../src/ui/fonts/font_vt323_22.h"
#include "../src/ui/glyph_draw.h"
#include <stdio.h>
#include <assert.h>

static bool glyph_nonblank(const Font *f, char c) {
    const uint8_t *g = f->bitmap +
        (unsigned)((unsigned char)c - f->first) * f->height * f->stride;
    for (unsigned i = 0; i < (unsigned) f->height * f->stride; i++)
        if (g[i]) return true;
    return false;
}

int main(void) {
    // Field sanity for the chosen sizes.
    assert(g_font_ps2p8.height == 8);
    assert(g_font_vt323_16.height == 16);
    assert(g_font_vt323_22.height == 22);
    assert(g_font_ps2p8.first == 0x20 && g_font_ps2p8.last == 0x7E);

    // Space is blank; letters are not.
    assert(!glyph_nonblank(&g_font_ps2p8, ' '));
    assert(glyph_nonblank(&g_font_ps2p8, 'A'));
    assert(glyph_nonblank(&g_font_vt323_16, 'A'));
    assert(glyph_nonblank(&g_font_vt323_22, 'g'));

    // Width math is consistent with the rasterizer.
    int w = gd_text_width(&g_font_ps2p8, 2, "AB");
    assert(w == 2 * g_font_ps2p8.width * 2);

    printf("test_fonts OK\n");
    return 0;
}
