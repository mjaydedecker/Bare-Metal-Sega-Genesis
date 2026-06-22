//
// src/menu/rom_layout.cpp
//
// Bare Metal Sega Genesis
// See rom_layout.h.
//
#include "rom_layout.h"
#include <string.h>

ScrollThumb scrollbar_thumb(int track_h, int count, int visible, int top) {
    ScrollThumb t;
    if (count <= 0 || visible <= 0 || count <= visible) {
        t.y = 0; t.h = track_h;
        return t;
    }
    int h = track_h * visible / count;
    if (h < 8) h = 8;
    if (h > track_h) h = track_h;

    int span = count - visible;            // max value of top
    if (top < 0) top = 0;
    if (top > span) top = span;
    int y = (track_h - h) * top / span;
    if (y < 0) y = 0;
    if (y + h > track_h) y = track_h - h;

    t.y = y; t.h = h;
    return t;
}

int rom_ext_offset(const char *name) {
    if (name == 0) return -1;
    int n = (int) strlen(name);
    for (int i = n - 1; i >= 0; i--) {
        if (name[i] == '.') {
            if (i > 0 && i < n - 1) return i;
            return -1;
        }
    }
    return -1;
}
