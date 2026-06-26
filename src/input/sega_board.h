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
static const BoardPinMap kBoardPinMap =
{
    {
        {  4, {  5,  6,  7,  8,  9, 10 } },   // Port 1 (Player 1)
        { 11, { 12, 13, 16, 17, 22, 23 } },   // Port 2 (Player 2)
    }
};

// True if every GPIO number in the map is distinct.
bool pinmap_unique(const BoardPinMap &m);

#endif
