#include "../src/ui/hud.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    char lines[HUD_LINES][HUD_COLS + 1];

    HudStats s;
    s.fps = 59; s.underruns = 0; s.overruns = 2;
    s.queued = 1440; s.target = 2880;
    s.rom = "SD:/roms/Sonic.bin";
    s.mode = "1080p"; s.scale = "aspect";

    unsigned n = hud_format(s, lines, HUD_LINES);
    assert(n == 4);
    assert(strcmp(lines[0], "FPS 59  U 0  O 2") == 0);
    assert(strcmp(lines[1], "AQ 1440/2880") == 0);
    assert(strcmp(lines[2], "Sonic.bin") == 0);   // dir stripped
    assert(strcmp(lines[3], "1080p  aspect") == 0);
    for (unsigned i = 0; i < n; i++) assert(strlen(lines[i]) <= HUD_COLS);

    // Long ROM name is truncated to HUD_COLS, dir stripped.
    HudStats l = s;
    l.rom = "SD:/roms/A Very Long Game Name That Exceeds.bin";
    hud_format(l, lines, HUD_LINES);
    assert(strlen(lines[2]) == HUD_COLS);
    assert(strncmp(lines[2], "A Very Long Game Name", 21) == 0);

    // NULL rom -> "-"; NULL mode/scale safe.
    HudStats z; z.fps = 0; z.underruns = 0; z.overruns = 0;
    z.queued = 0; z.target = 0; z.rom = 0; z.mode = 0; z.scale = 0;
    unsigned zn = hud_format(z, lines, HUD_LINES);
    assert(zn == 4);
    assert(strcmp(lines[0], "FPS 0  U 0  O 0") == 0);
    assert(strcmp(lines[1], "AQ 0/0") == 0);
    assert(strcmp(lines[2], "-") == 0);
    for (unsigned i = 0; i < zn; i++) assert(strlen(lines[i]) <= HUD_COLS);

    // Respects max_lines.
    assert(hud_format(s, lines, 2) == 2);

    printf("All hud tests passed\n");
    return 0;
}
