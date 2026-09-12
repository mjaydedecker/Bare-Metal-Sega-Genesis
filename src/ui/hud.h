//
// src/ui/hud.h
//
// Bare Metal Sega Genesis
// Pure formatter for the diagnostics HUD. Turns runtime stats into labelled
// cells with health classification. No Circle dependency — host-testable.
//

#ifndef _ui_hud_h
#define _ui_hud_h

#include <stddef.h>

#define HUD_COLS  22
#define HUD_LINES 4

#define HUD_LABEL_MAX 6
#define HUD_VALUE_MAX 18
#define HUD_CELL_MAX  9

struct HudStats
{
    unsigned    fps;        // measured frames/sec
    unsigned    underruns;  // audio underrun count
    unsigned    overruns;   // audio overrun count
    unsigned    queued;     // audio frames currently queued
    unsigned    target;     // pacing target queue depth
    const char *rom;        // ROM path or name (may be NULL); dir is stripped
    const char *mode;       // "native"/"1080p"/... (may be NULL)
    const char *scale;      // "integer"/"stretch"/"aspect" (may be NULL)
    bool        vsync;      // tear-free page flip on/off
    bool        widescreen; // widescreen on/off
    const char *latency;    // "low" | "medium" | "high" (may be NULL)
};

enum HudHealth { HUD_GOOD, HUD_WARN, HUD_BAD, HUD_INFO };

struct HudCell
{
    char      label[HUD_LABEL_MAX + 1];
    char      value[HUD_VALUE_MAX + 1];
    HudHealth health;
};

// Fill up to max cells (fixed order: FPS, AQ, UR, OR, ROM, MODE, VSYNC, WIDE,
// LAT). Returns the number written.
unsigned hud_build(const HudStats &s, HudCell cells[], unsigned max);

#endif
