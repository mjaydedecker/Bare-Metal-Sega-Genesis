//
// src/input/pad_reconcile.h
//
// Bare Metal Sega Genesis
// Pure per-port hotplug decision: compare a controller port's cached device
// pointer against the pointer the name service currently returns. No Circle
// dependency so it can be host-tested; the caller performs the side effects.
//

#ifndef _input_pad_reconcile_h
#define _input_pad_reconcile_h

enum class PadAction
{
    Keep,      // no change (same device, or still absent)
    Clear,     // device removed -> zero the cache and drop the slot
    Acquire    // new device present (first plug, re-plug, or swap) -> register + cache
};

// cached  = the port's last-known device pointer (0 if none)
// queried = what GetDevice("upad", port+1) returns now (0 if absent/removed)
PadAction pad_reconcile(const void *cached, const void *queried);

#endif
