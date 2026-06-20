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

    printf("All menu_state tests passed\n");
    return 0;
}
