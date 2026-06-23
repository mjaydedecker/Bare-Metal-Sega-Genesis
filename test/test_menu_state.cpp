#include "../src/menu/menu_state.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // 5 entries, window of 3 rows (indices 0,1,2 visible initially).
    MenuState a = {5, 0, 0, 3};
    menu_move(&a, -1); assert(a.selected == 0 && a.top == 0);   // clamp at top
    menu_move(&a, +1); assert(a.selected == 1 && a.top == 0);
    menu_move(&a, +1); assert(a.selected == 2 && a.top == 0);
    menu_move(&a, +1); assert(a.selected == 3 && a.top == 1);   // scroll down
    menu_move(&a, +1); assert(a.selected == 4 && a.top == 2);
    menu_move(&a, +1); assert(a.selected == 4 && a.top == 2);   // clamp at bottom
    menu_move(&a, -3); assert(a.selected == 1 && a.top == 1);   // scroll up
    menu_move(&a, -1); assert(a.selected == 0 && a.top == 0);

    // Fewer entries than the window: top stays 0.
    MenuState b = {2, 0, 0, 10};
    menu_move(&b, +1); assert(b.selected == 1 && b.top == 0);

    // Empty list: nothing moves, no out-of-range.
    MenuState c = {0, 0, 0, 5};
    menu_move(&c, +1); assert(c.selected == 0 && c.top == 0);

    // menu_next_enabled: pause-menu layout (Resume, -, -, Reset, -, Return).
    const bool en[6] = {true, false, false, true, false, true};
    assert(menu_next_enabled(en, 6, 0, +1) == 3);   // skip 1,2
    assert(menu_next_enabled(en, 6, 3, +1) == 5);   // skip 4
    assert(menu_next_enabled(en, 6, 5, +1) == 5);   // none after -> stay
    assert(menu_next_enabled(en, 6, 5, -1) == 3);
    assert(menu_next_enabled(en, 6, 3, -1) == 0);
    assert(menu_next_enabled(en, 6, 0, -1) == 0);   // none before -> stay
    const bool none[2] = {false, false};
    assert(menu_next_enabled(none, 2, 0, +1) == 0); // all disabled -> stay

    // list_window: fits entirely (visible clamps to count, top 0).
    ListWindow w = list_window(400, 30, 9, 0);
    assert(w.visible == 9 && w.top == 0);

    // 480p settings: avail = 480-96-40-12 = 332, row 30 -> 11 visible of 16.
    w = list_window(332, 30, 16, 0);   assert(w.visible == 11 && w.top == 0);
    w = list_window(332, 30, 16, 10);  assert(w.visible == 11 && w.top == 0);  // still fits
    w = list_window(332, 30, 16, 11);  assert(w.visible == 11 && w.top == 1);  // scroll
    w = list_window(332, 30, 16, 15);  assert(w.visible == 11 && w.top == 5);  // last row
    // top never exceeds count-visible.
    assert(list_window(332, 30, 16, 15).top == 16 - 11);

    // 480p hotkeys: 12 rows, 11 visible -> last row scrolls one.
    w = list_window(332, 30, 12, 11);  assert(w.visible == 11 && w.top == 1);
    w = list_window(332, 30, 12, 0);   assert(w.visible == 11 && w.top == 0);

    // guards: row_h <= 0, count < 1.
    w = list_window(332, 0, 16, 3);    assert(w.visible == 16 && w.top == 0);  // row_h<=0 -> all
    w = list_window(332, 30, 0, 0);    assert(w.visible == 0 && w.top == 0);   // empty list
    w = list_window(10, 30, 16, 0);    assert(w.visible == 1 && w.top == 0);   // <1 row fits -> 1

    printf("All menu_state tests passed\n");
    return 0;
}
