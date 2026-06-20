//
// src/menu/menu_state.cpp
//
// Bare Metal Sega Genesis
// See menu_state.h.
//

#include "menu_state.h"

void menu_move(MenuState *s, int delta)
{
    if (s == 0) return;
    if (s->count <= 0) { s->selected = 0; s->top = 0; return; }

    int sel = s->selected + delta;
    if (sel < 0) sel = 0;
    if (sel > s->count - 1) sel = s->count - 1;
    s->selected = sel;

    int rows = s->visible_rows > 0 ? s->visible_rows : 1;
    if (s->selected < s->top)
        s->top = s->selected;                       // scrolled above the window
    else if (s->selected > s->top + rows - 1)
        s->top = s->selected - rows + 1;            // scrolled below the window

    if (s->top < 0) s->top = 0;
    int maxTop = s->count - rows;
    if (maxTop < 0) maxTop = 0;
    if (s->top > maxTop) s->top = maxTop;
}

int menu_next_enabled(const bool *enabled, int count, int from, int dir)
{
    if (count <= 0) return from;
    int step = (dir >= 0) ? 1 : -1;
    for (int i = from + step; i >= 0 && i < count; i += step)
    {
        if (enabled[i]) return i;
    }
    return from;
}
