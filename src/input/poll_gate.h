//
// src/input/poll_gate.h
//
// Bare Metal Sega Genesis
// Minimum-interval gate for GPIO pad polling. A 6-button Sega pad needs SELECT
// held idle ~1.5 ms between polls to reset its phase counter, but menus read
// input several times back-to-back, so GpioPads::Poll() skips a poll that comes
// too soon and keeps its cached (still fresh) result. Pure, no Circle deps.
//

#ifndef _input_poll_gate_h
#define _input_poll_gate_h

struct PollGate
{
    bool     primed;    // false until the first due poll
    unsigned last_us;   // clock of the last due poll
};

// True (and records now_us) if nothing has been polled yet or at least min_us
// has elapsed since the last due poll. Unsigned subtraction handles the 32-bit
// microsecond clock wrapping.
static inline bool poll_gate_due(PollGate &g, unsigned now_us, unsigned min_us)
{
    if (g.primed && (unsigned) (now_us - g.last_us) < min_us)
        return false;
    g.primed  = true;
    g.last_us = now_us;
    return true;
}

#endif
