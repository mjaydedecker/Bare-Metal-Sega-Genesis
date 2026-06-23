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

ListWindow list_window(int avail_h, int row_h, int count, int selected)
{
    ListWindow w = { 0, 0 };
    if (count < 1) return w;

    int visible = (row_h > 0) ? avail_h / row_h : count;
    if (visible < 1)      visible = 1;
    if (visible > count)  visible = count;

    int top = 0;
    if (selected >= visible)        top = selected - visible + 1;
    if (top > count - visible)      top = count - visible;
    if (top < 0)                    top = 0;

    w.visible = visible;
    w.top     = top;
    return w;
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
