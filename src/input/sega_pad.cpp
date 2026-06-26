//
// src/input/sega_pad.cpp
//
#include "sega_pad.h"

enum { D_UP = 0, D_DOWN = 1, D_LEFT = 2, D_RIGHT = 3, D_TL = 4, D_TR = 5 };

static inline bool pressed(uint8_t data, int bit)
{
    return ((data >> bit) & 1u) == 0u;   // active-low: 0 means pressed
}

SegaDecoded sega_decode(const SegaSample phases[SEGA_PHASES])
{
    SegaDecoded out;
    out.type = SegaPadType::None;
    out.buttons = 0;

    // Presence: any line low on any phase = something responded.
    bool anyResponse = false;
    for (int i = 0; i < SEGA_PHASES; ++i)
        if ((phases[i].data & 0x3F) != 0x3F) { anyResponse = true; break; }
    if (!anyResponse)
        return out;   // None

    // Genesis signature: at a TH=low read, Left+Right are forced low.
    const SegaSample &lo = phases[1];
    bool genesis = pressed(lo.data, D_LEFT) && pressed(lo.data, D_RIGHT);
    if (!genesis) { out.type = SegaPadType::Unsupported; return out; }

    // D-pad + B/C from the TH=high read.
    const SegaSample &hi = phases[0];
    if (pressed(hi.data, D_UP))    out.buttons |= GP_UP;
    if (pressed(hi.data, D_DOWN))  out.buttons |= GP_DOWN;
    if (pressed(hi.data, D_LEFT))  out.buttons |= GP_LEFT;
    if (pressed(hi.data, D_RIGHT)) out.buttons |= GP_RIGHT;
    if (pressed(hi.data, D_TL))    out.buttons |= GP_B;    // B
    if (pressed(hi.data, D_TR))    out.buttons |= GP_RB;   // C

    // A + Start from the TH=low read (L/R are forced low here, ignore them).
    if (pressed(lo.data, D_TL))    out.buttons |= GP_A;     // A
    if (pressed(lo.data, D_TR))    out.buttons |= GP_START; // Start

    out.type = SegaPadType::ThreeButton;   // 6-button upgrade added in Task 3
    return out;
}
