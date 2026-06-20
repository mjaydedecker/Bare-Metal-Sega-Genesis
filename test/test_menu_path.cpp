#include "../src/menu/menu_path.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    char out[300];

    path_join("SD:/roms", "Genesis", out, sizeof out);
    assert(strcmp(out, "SD:/roms/Genesis") == 0);
    path_join("SD:/roms/Genesis", "Sonic the Hedgehog.md", out, sizeof out);
    assert(strcmp(out, "SD:/roms/Genesis/Sonic the Hedgehog.md") == 0);

    path_parent("SD:/roms/Genesis", "SD:/roms", out, sizeof out);
    assert(strcmp(out, "SD:/roms") == 0);
    path_parent("SD:/roms/A/B", "SD:/roms", out, sizeof out);
    assert(strcmp(out, "SD:/roms/A") == 0);
    path_parent("SD:/roms", "SD:/roms", out, sizeof out);   // floored at root
    assert(strcmp(out, "SD:/roms") == 0);

    // Alias-safe: out == input buffer.
    char p[300];
    strcpy(p, "SD:/roms/A");
    path_parent(p, "SD:/roms", p, sizeof p);
    assert(strcmp(p, "SD:/roms") == 0);
    strcpy(p, "SD:/roms");
    path_join(p, "Genesis", p, sizeof p);
    assert(strcmp(p, "SD:/roms/Genesis") == 0);

    printf("All menu_path tests passed\n");
    return 0;
}
