#include "../src/input/sega_board.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // The shipped pin map has no pin assigned twice.
    assert(pinmap_unique(kBoardPinMap));

    // Sanity: the documented assignment (P1 select=22, P2 select=12).
    assert(kBoardPinMap.port[0].select == 22);
    assert(kBoardPinMap.port[1].select == 12);
    assert(kBoardPinMap.port[0].data[0] == 10);
    assert(kBoardPinMap.port[1].data[5] == 7);

    // A map with a duplicated pin is rejected.
    BoardPinMap bad = kBoardPinMap;
    bad.port[1].data[0] = 22;   // collide with port0 select
    assert(!pinmap_unique(bad));

    printf("test_sega_board: OK\n");
    return 0;
}
