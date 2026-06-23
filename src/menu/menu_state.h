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

struct ListWindow { int visible; int top; };

// Stateless scroll window for a fixed-length list: given the pixel height
// available for rows (`avail_h`), the per-row height (`row_h`), the total
// `count`, and the currently `selected` index, returns how many rows fit
// (`visible`, clamped to [1,count]) and the first visible row index (`top`,
// scrolled so `selected` stays on screen). Used by menu screens so long lists
// don't overflow short framebuffers (e.g. 480p). Returns {0,0} for count < 1.
ListWindow list_window(int avail_h, int row_h, int count, int selected);

#endif
