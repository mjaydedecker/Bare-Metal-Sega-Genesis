#include "../src/ui/theme.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    // RGB565 macro: bg #0a0b10 -> R=1,G=2,B=2 -> 0x0842
    assert(RGB565(0x0a, 0x0b, 0x10) == 0x0842);
    assert(theme::BG == 0x0842);
    // pure white passes through
    assert(RGB565(0xff, 0xff, 0xff) == 0xFFFF);
    assert(theme::WHITE == 0xFFFF);
    // accent red #E11B22 -> R=28,G=6,B=4 -> (28<<11)|(6<<5)|4 = 0xE0C4
    assert(theme::SELECTION == 0xE0C4);
    // distinct roles really are distinct
    assert(theme::VALUE != theme::ADJUST);
    assert(theme::ACTIVE != theme::SELECTION);
    printf("test_theme OK\n");
    return 0;
}
