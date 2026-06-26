//
// src/input/sega_sequence.cpp
//
#include "sega_sequence.h"

void sega_run_sequence(const SegaIo &io, SegaSample out[SEGA_PHASES])
{
    for (unsigned i = 0; i < SEGA_PHASES; ++i)
    {
        unsigned sel = (i & 1) ? 0u : 1u;       // hi, lo, hi, lo, ...
        io.set_select(io.ctx, sel);
        io.settle(io.ctx);
        uint8_t data = 0;
        for (unsigned d = 0; d < 6; ++d)
            data |= (uint8_t)((io.read_pin(io.ctx, d) & 1u) << d);
        out[i].sel  = (uint8_t) sel;
        out[i].data = data;
    }
    io.set_select(io.ctx, 1);   // leave SELECT idle high (lets 6-btn counter reset)
}
