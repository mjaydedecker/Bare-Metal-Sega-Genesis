#include "../src/video/splash.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    // Valid 2x2 SGSP buffer.
    unsigned char b[8 + 2 * 2 * 2];
    memcpy(b, "SGSP", 4);
    b[4] = 2; b[5] = 0; b[6] = 2; b[7] = 0;       // w=2, h=2 (LE)
    for (int i = 0; i < 8; i++) b[8 + i] = (unsigned char) i;

    SplashImage img;
    assert(splash_parse(b, sizeof b, &img));
    assert(img.w == 2 && img.h == 2);
    assert(img.pixels == (const unsigned short *) (b + 8));

    // Bad magic -> reject.
    unsigned char bad[sizeof b]; memcpy(bad, b, sizeof b); bad[0] = 'X';
    assert(!splash_parse(bad, sizeof bad, &img));

    // Wrong length (one byte short) -> reject.
    assert(!splash_parse(b, sizeof b - 1, &img));
    // Shorter than the 8-byte header -> reject.
    assert(!splash_parse(b, 4, &img));

    // Zero dimensions -> reject.
    unsigned char z[8]; memcpy(z, "SGSP", 4);
    z[4] = z[5] = z[6] = z[7] = 0;
    assert(!splash_parse(z, sizeof z, &img));

    // splash_scale: 1280x720 fb, 160x100 img -> 5 (6 would exceed 70%).
    assert(splash_scale(1280, 720, 160, 100) == 5);
    // Image bigger than 70% even at scale 1 -> 1.
    assert(splash_scale(100, 100, 200, 200) == 1);
    // Tiny image on large fb -> at least 1.
    assert(splash_scale(1920, 1080, 16, 16) >= 1);
    // Zero image dimension -> 1.
    assert(splash_scale(1280, 720, 0, 100) == 1);

    printf("All splash tests passed\n");
    return 0;
}
