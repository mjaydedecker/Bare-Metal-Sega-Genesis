//
// src/input/pad_reconcile.cpp
//
// Bare Metal Sega Genesis
// See pad_reconcile.h.
//

#include "pad_reconcile.h"

PadAction pad_reconcile(const void *cached, const void *queried)
{
    if (queried == 0)
    {
        return cached != 0 ? PadAction::Clear : PadAction::Keep;
    }
    return queried != cached ? PadAction::Acquire : PadAction::Keep;
}
