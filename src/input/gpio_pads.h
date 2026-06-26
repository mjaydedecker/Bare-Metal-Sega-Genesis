//
// src/input/gpio_pads.h
//
// Bare Metal Sega Genesis
// Reads real Sega DB9 controllers wired directly to GPIO (per sega_board.h).
// Drives SELECT and samples the 6 data lines across SEGA_PHASES, feeding the
// pure sega_decode(). Mirrors Gamepad's shape so it can be a second source of
// the per-port GP_* bitmask. Pads are powered at 3.3V (see HAT electrical
// contract); data inputs use internal pull-ups (1=released, 0=pressed).
//

#ifndef _input_gpio_pads_h
#define _input_gpio_pads_h

#include <circle/gpiopin.h>
#include <circle/types.h>
#include "sega_pad.h"
#include "sega_board.h"

class GpioPads
{
public:
    static const unsigned NUM_PORTS = 2;

    GpioPads(void);

    // Configure the GPIO pin modes from kBoardPinMap. Call once after boot.
    void Init(void);

    // Run the SELECT sequence for both ports, decode, and cache. Call per frame.
    void Poll(void);

    unsigned    Buttons(unsigned port) const;     // GP_* mask (0 if invalid)
    boolean     IsPresent(unsigned port) const;
    SegaPadType PadTypeAt(unsigned port) const;

private:
    void PollPort(unsigned port, SegaSample out[SEGA_PHASES]);

    CGPIOPin    m_Select[NUM_PORTS];
    CGPIOPin    m_Data[NUM_PORTS][6];
    unsigned    m_Buttons[NUM_PORTS];
    SegaPadType m_Type[NUM_PORTS];
};

#endif
