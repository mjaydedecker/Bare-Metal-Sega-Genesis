#include "../src/input/input_merge.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(merge_buttons(0, 0) == 0);
    assert(merge_buttons(0x1, 0x0) == 0x1);   // USB only
    assert(merge_buttons(0x0, 0x2) == 0x2);   // GPIO only
    assert(merge_buttons(0x1, 0x2) == 0x3);   // both sources OR together
    assert(merge_buttons(0x5, 0x4) == 0x5);   // overlap is idempotent
    printf("test_input_merge: OK\n");
    return 0;
}
