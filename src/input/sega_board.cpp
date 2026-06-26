//
// src/input/sega_board.cpp
//
#include "sega_board.h"

bool pinmap_unique(const BoardPinMap &m)
{
    unsigned all[14];
    unsigned n = 0;
    for (int p = 0; p < 2; ++p)
    {
        all[n++] = m.port[p].select;
        for (int d = 0; d < 6; ++d)
            all[n++] = m.port[p].data[d];
    }
    for (unsigned i = 0; i < n; ++i)
        for (unsigned j = i + 1; j < n; ++j)
            if (all[i] == all[j])
                return false;
    return true;
}
