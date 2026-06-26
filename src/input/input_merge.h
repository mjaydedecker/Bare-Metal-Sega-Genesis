//
// src/input/input_merge.h
//
// Bare Metal Sega Genesis
// Collision rule for coexisting input sources (USB + GPIO) on one port.
// Decision (spec M3): OR both sources. A port rarely has both a USB and a DB9
// pad at once, so OR is identical in practice while avoiding a presence-priority
// edge case. This is the single place that rule lives. Pure, no Circle deps.
//

#ifndef _input_input_merge_h
#define _input_input_merge_h

static inline unsigned merge_buttons(unsigned usb, unsigned gpio)
{
    return usb | gpio;
}

#endif
