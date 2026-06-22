#include "../src/menu/rom_layout.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    // everything fits -> full track
    ScrollThumb a = scrollbar_thumb(100, 5, 10, 0);
    assert(a.y == 0 && a.h == 100);

    // 20 items, 10 visible, at top -> half-height thumb at top
    ScrollThumb b = scrollbar_thumb(100, 20, 10, 0);
    assert(b.h == 50 && b.y == 0);

    // scrolled to the bottom -> thumb flush with the track bottom
    ScrollThumb c = scrollbar_thumb(100, 20, 10, 10);  // top can reach count-visible
    assert(c.y + c.h == 100);

    // thumb never smaller than 8px
    ScrollThumb d = scrollbar_thumb(100, 1000, 10, 0);
    assert(d.h >= 8);

    // extension offset
    assert(rom_ext_offset("Sonic.md") == 5);
    assert(rom_ext_offset("a.b.gen") == 3);   // last dot
    assert(rom_ext_offset("noext") == -1);
    assert(rom_ext_offset(".hidden") == -1);  // nothing before the dot
    assert(rom_ext_offset("trailing.") == -1);// nothing after the dot
    assert(rom_ext_offset("") == -1);

    printf("test_rom_layout OK\n");
    return 0;
}
