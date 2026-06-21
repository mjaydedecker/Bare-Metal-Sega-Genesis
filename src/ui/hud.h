//
// src/ui/hud.h
//
// Bare Metal Sega Genesis
// Pure formatter for the diagnostics HUD. Turns runtime stats into fixed-width
// text lines. No Circle dependency — host-testable.
//

#ifndef _ui_hud_h
#define _ui_hud_h

#include <stddef.h>

#define HUD_COLS  22
#define HUD_LINES 4

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
};

// Fill up to max_lines NUL-terminated lines (each <= HUD_COLS chars). Returns
// the number of lines written.
unsigned hud_format(const HudStats &s, char lines[][HUD_COLS + 1],
                    unsigned max_lines);

#endif
