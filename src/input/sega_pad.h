//
// src/input/sega_pad.h
//
// Bare Metal Sega Genesis
// Pure SELECT-multiplex decoder for Sega DB9 pads. A full poll captures
// SEGA_PHASES alternating-SELECT samples (hi,lo,hi,lo,...) — enough to cover the
// 6-button sequence. Data bits are stored AS READ (pull-ups: 1=released,
// 0=pressed); the decoder outputs an active-high GP_* bitmask. No Circle deps.
//

#ifndef _input_sega_pad_h
#define _input_sega_pad_h

#include <stdint.h>
#include "joypad_map.h"   // GP_* bit constants (macros only)

#define SEGA_PHASES 8

struct SegaSample
{
    uint8_t sel;    // 1 = SELECT high, 0 = SELECT low (as driven)
    uint8_t data;   // low 6 bits = D0..D5 as read (1=released, 0=pressed)
};

enum class SegaPadType : uint8_t
{
    None,          // no pad / no response (all lines released every phase)
    Unsupported,   // responds but not a Genesis pad (no L/R-forced-low signature)
    ThreeButton,
    SixButton,
};

struct SegaDecoded
{
    SegaPadType type;
    unsigned    buttons;   // GP_* bitmask, 1 = pressed
};

// Decode one full 8-phase poll into a pad type + GP_* bitmask.
SegaDecoded sega_decode(const SegaSample phases[SEGA_PHASES]);

#endif
