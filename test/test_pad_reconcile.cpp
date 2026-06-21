#include "../src/input/pad_reconcile.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    int a, b;   // distinct addresses serve as fake device pointers

    // Never present, still absent -> nothing to do.
    assert(pad_reconcile(0, 0) == PadAction::Keep);

    // First plug-in: nothing cached, a device appears.
    assert(pad_reconcile(0, &a) == PadAction::Acquire);

    // Unchanged: the same device is still present.
    assert(pad_reconcile(&a, &a) == PadAction::Keep);

    // Removed: we had a device, now GetDevice returns NULL.
    assert(pad_reconcile(&a, 0) == PadAction::Clear);

    // Re-plug / swap at the same index: a different pointer.
    assert(pad_reconcile(&a, &b) == PadAction::Acquire);

    printf("test_pad_reconcile: OK\n");
    return 0;
}
