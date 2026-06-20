//
// src/menu/menu_state.h
//
// Bare Metal Sega Genesis
// Pure list selection + scroll-window math. No Circle deps (host-testable).
//

#ifndef _menu_menu_state_h
#define _menu_menu_state_h

struct MenuState
{
    int count;         // number of entries
    int selected;      // 0..count-1
    int top;           // index of the first visible row
    int visible_rows;  // rows that fit on screen
};

// Move selection by delta, clamp to [0,count-1], and scroll `top` so the
// selection stays within [top, top+visible_rows-1]. Safe for count <= 0.
void menu_move(MenuState *s, int delta);

// Starting from `from`, return the next index in direction `dir` (+1 or -1)
// whose enabled[] is true. No wrap: if there is no enabled entry that way (or
// none at all), returns `from`.
int menu_next_enabled(const bool *enabled, int count, int from, int dir);

#endif
