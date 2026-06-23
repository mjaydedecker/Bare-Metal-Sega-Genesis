#include "../src/ui/hud.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static const HudCell *find(const HudCell *c, unsigned n, const char *label)
{
    for (unsigned i = 0; i < n; i++) if (strcmp(c[i].label, label) == 0) return &c[i];
    return 0;
}

int main(void)
{
    HudCell cells[HUD_CELL_MAX];

    HudStats s;
    s.fps = 60; s.underruns = 0; s.overruns = 2;
    s.queued = 1440; s.target = 2880;
    s.rom = "SD:/roms/Sonic.bin"; s.mode = "1080p"; s.scale = "aspect";
    s.vsync = false; s.widescreen = true; s.latency = "medium";

    unsigned n = hud_build(s, cells, HUD_CELL_MAX);
    assert(n == 9);

    // FPS healthy at 60.
    const HudCell *fps = find(cells, n, "FPS");
    assert(fps && strcmp(fps->value, "60") == 0 && fps->health == HUD_GOOD);

    // Underruns 0 -> good; overruns 2 -> bad.
    assert(find(cells, n, "UR")->health == HUD_GOOD);
    assert(strcmp(find(cells, n, "UR")->value, "0") == 0);
    assert(find(cells, n, "OR")->health == HUD_BAD);
    assert(strcmp(find(cells, n, "OR")->value, "2") == 0);

    // AQ queued/target, info.
    assert(strcmp(find(cells, n, "AQ")->value, "1440/2880") == 0);
    assert(find(cells, n, "AQ")->health == HUD_INFO);

    // ROM dir stripped.
    assert(strcmp(find(cells, n, "ROM")->value, "Sonic.bin") == 0);

    // MODE = mode + space + scale.
    assert(strcmp(find(cells, n, "MODE")->value, "1080p aspect") == 0);

    // New state fields.
    assert(strcmp(find(cells, n, "VSYNC")->value, "off") == 0);
    assert(strcmp(find(cells, n, "WIDE")->value, "on") == 0);
    assert(strcmp(find(cells, n, "LAT")->value, "medium") == 0);

    // FPS health thresholds: 58 good, 57 warn, 50 warn, 49 bad.
    HudStats h = s;
    h.fps = 58; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_GOOD);
    h.fps = 57; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_WARN);
    h.fps = 50; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_WARN);
    h.fps = 49; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_BAD);

    // Long ROM name truncated to HUD_VALUE_MAX, dir stripped.
    HudStats l = s;
    l.rom = "SD:/roms/A Very Long Game Name That Exceeds.bin";
    hud_build(l, cells, HUD_CELL_MAX);
    assert(strlen(find(cells, 9, "ROM")->value) == HUD_VALUE_MAX);

    // NULL rom -> "-"; NULL mode/scale safe.
    HudStats z; z.fps = 0; z.underruns = 0; z.overruns = 0;
    z.queued = 0; z.target = 0; z.rom = 0; z.mode = 0; z.scale = 0;
    z.vsync = true; z.widescreen = false; z.latency = "low";
    unsigned zn = hud_build(z, cells, HUD_CELL_MAX);
    assert(zn == 9);
    assert(strcmp(find(cells, zn, "ROM")->value, "-") == 0);
    assert(strcmp(find(cells, zn, "VSYNC")->value, "on") == 0);

    // Bounds: every label/value within caps.
    for (unsigned i = 0; i < zn; i++) {
        assert(strlen(cells[i].label) <= HUD_LABEL_MAX);
        assert(strlen(cells[i].value) <= HUD_VALUE_MAX);
    }

    // Respects max.
    assert(hud_build(s, cells, 3) == 3);

    // hud_scale: integer scale from framebuffer height, always >= 1.
    assert(hud_scale(240)  == 1);
    assert(hud_scale(480)  == 1);
    assert(hud_scale(540)  == 1);
    assert(hud_scale(720)  == 1);
    assert(hud_scale(1080) == 2);
    assert(hud_scale(1440) == 2);
    assert(hud_scale(0)    == 1);   // guard: never zero

    printf("All hud tests passed\n");
    return 0;
}
