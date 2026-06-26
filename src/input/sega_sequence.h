//
// src/input/sega_sequence.h
//
// Bare Metal Sega Genesis
// Pure SELECT-toggle sequencing for a Sega DB9 pad, isolated from the GPIO
// hardware via an injected SegaIo. Produces the SEGA_PHASES sample array that
// sega_decode() consumes. In production SegaIo wraps CGPIOPin + CTimer; in host
// tests it wraps a simulated pad. No Circle deps — host-testable.
//

#ifndef _input_sega_sequence_h
#define _input_sega_sequence_h

#include "sega_pad.h"   // SegaSample, SEGA_PHASES

// Hardware abstraction injected by the caller.
//   set_select(ctx, level) — drive the SELECT line (0 or 1)
//   settle(ctx)            — wait for the pad's mux to settle after a toggle
//   read_pin(ctx, d)       — read data line d (0..5), returns 0 or 1
struct SegaIo
{
    void     (*set_select)(void *ctx, unsigned level);
    void     (*settle)(void *ctx);
    unsigned (*read_pin)(void *ctx, unsigned d);
    void      *ctx;
};

// Drive the full SELECT sequence (hi,lo,hi,lo,... for SEGA_PHASES), sampling the
// six data lines into each phase. Leaves SELECT idle high so the pad's 6-button
// counter resets before the next poll. Pure with respect to io.
void sega_run_sequence(const SegaIo &io, SegaSample out[SEGA_PHASES]);

#endif
