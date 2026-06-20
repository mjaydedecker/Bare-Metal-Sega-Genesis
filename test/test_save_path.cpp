#include "../src/menu/save_path.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    char out[300];

    state_path("SD:/roms/Sonic.md", 2, out, sizeof out);
    assert(strcmp(out, "SD:/saves/Sonic.md.state2") == 0);

    state_path("SD:/roms/Genesis/Streets of Rage.md", 1, out, sizeof out);
    assert(strcmp(out, "SD:/saves/Streets of Rage.md.state1") == 0);

    state_path("Game.bin", 4, out, sizeof out);   // no directory
    assert(strcmp(out, "SD:/saves/Game.bin.state4") == 0);

    sram_path("SD:/roms/Sonic.md", out, sizeof out);
    assert(strcmp(out, "SD:/saves/Sonic.md.srm") == 0);

    sram_path("SD:/roms/Genesis/Phantasy Star.md", out, sizeof out);
    assert(strcmp(out, "SD:/saves/Phantasy Star.md.srm") == 0);

    sram_path("Game.bin", out, sizeof out);        // no directory
    assert(strcmp(out, "SD:/saves/Game.bin.srm") == 0);

    printf("All save_path tests passed\n");
    return 0;
}
