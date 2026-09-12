#include "../src/input/poll_gate.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const unsigned MIN = 2000u;   // 2 ms

    // Never polled yet: the first call is always due.
    PollGate g = { false, 0u };
    assert(poll_gate_due(g, 5000u, MIN));

    // 1 ms later is too soon.
    assert(!poll_gate_due(g, 6000u, MIN));

    // A skipped call must not push the window forward: 2 ms after the last
    // DUE poll (5000) is due, even though a call was skipped at 6000.
    assert(poll_gate_due(g, 7000u, MIN));

    // Boundary: one microsecond short, then exactly the interval.
    assert(!poll_gate_due(g, 8999u, MIN));
    assert(poll_gate_due(g, 9000u, MIN));

    // The 32-bit microsecond clock wraps; unsigned difference stays correct.
    PollGate w = { true, 0xFFFFFF00u };
    assert(!poll_gate_due(w, 0x00000100u, MIN));   // 512 us later
    assert(poll_gate_due(w, 0x00000700u, MIN));    // 2048 us later

    printf("test_poll_gate: OK\n");
    return 0;
}
