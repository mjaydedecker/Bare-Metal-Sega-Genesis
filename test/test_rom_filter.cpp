#include "../src/menu/rom_filter.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(is_rom_filename("Sonic.md"));
    assert(is_rom_filename("GAME.BIN"));
    assert(is_rom_filename("x.gen"));
    assert(is_rom_filename("Long Game Name.MD"));   // long name, mixed case
    assert(!is_rom_filename("readme.txt"));
    assert(!is_rom_filename("noext"));
    assert(!is_rom_filename(".md"));                // bare extension, no stem
    assert(!is_rom_filename(""));
    assert(!is_rom_filename(0));

    printf("All rom_filter tests passed\n");
    return 0;
}
