//
// src/ui/icons.h
//
// Bare Metal Sega Genesis
// Pure FillRect/glyph-based UI icons for hint bars and selection markers.
//
#ifndef _ui_icons_h
#define _ui_icons_h
#include <stdint.h>
#include "font.h"

void icon_play (uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                int x, int y, int size, uint16_t color);
void icon_cross(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                int x, int y, int size, uint16_t color);
void icon_button(uint16_t *buf, unsigned pitchPx, int fbw, int fbh,
                 int x, int y, int d, char letter,
                 uint16_t fill, uint16_t fg, const Font *f);

#endif
