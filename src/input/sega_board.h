//
// src/input/sega_board.h
//
// Bare Metal Sega Genesis
// Physical GPIO pin assignment for the two DB9 controller ports (Pi 2, BCM
// numbering) — the ONE place that knows physical pins. The separate HAT project
// builds to this table + the 3.3V electrical contract. Data lines D0..D5 map to
// DB9 pins 1,2,3,4,6,9 (Up, Down, Left, Right, TL[B/A], TR[C/Start]).
//

#ifndef _input_sega_board_h
#define _input_sega_board_h

struct PortPins
{
    unsigned select;    // DB9 pin 7 — SELECT (output)
    unsigned data[6];   // D0..D5 (inputs, internal pull-up)
};

struct BoardPinMap
{
    PortPins port[2];
};

// Reserved (do not use): 18-21 (I2S), 14/15 (UART), 2/3 (I2C), 0/1 (HAT EEPROM).
// Spare (available, not currently used): 8, 12, 23, 25.
//
// Chosen so each port's 7 signals sit on physically adjacent GPIO header
// columns, grouped toward the side of the header nearest that port's DB9
// connector on the HAT (Port 1 -> lower-numbered columns, Port 2 ->
// higher-numbered columns), in the exact left-to-right order those 7
// signals appear on the connector's own physical pins -- this lets the
// HAT's PCB route both ports with zero same-layer trace crossings, instead
// of fighting a mismatched assignment with clever routing.
//
// Updated 2026-07-22 (second time): the HAT's DB9 connectors were corrected
// from female sockets to male pins (Genesis controllers have female cable
// ends), which mirrors the connector's physical pin order left-to-right.
// The previous assignment was matched to the old (wrong, female) pin order
// and had to be re-derived to match the male one; GPIO12/23 moved to spare,
// GPIO9/11 moved out of spare. See the HAT repo's
// docs/reviews/2026-07-22-pinmap-reassignment.md for the full analysis.
static const BoardPinMap kBoardPinMap =
{
    {
        { 22, {  4, 27, 24, 10, 17,  9 } },   // Port 1 (Player 1)
        {  6, { 11,  5, 13, 16,  7, 26 } },   // Port 2 (Player 2)
    }
};

// True if every GPIO number in the map is distinct.
bool pinmap_unique(const BoardPinMap &m);

#endif
