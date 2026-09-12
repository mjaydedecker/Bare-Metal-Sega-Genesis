//
// src/menu/pad_test.cpp
//
// Bare Metal Sega Genesis
// See pad_test.h.
//

#include "pad_test.h"
#include "../input/joypad_map.h"   // GP_* bits

// Order must match PadTestKeyIndex. C/Z/Mode sit on the bits sega_decode emits
// (and a calibrated USB pad's shoulder/select buttons).
const PadTestKey PAD_TEST_KEYS[PAD_TEST_NUM_KEYS] =
{
    { "Up",    GP_UP,     false },
    { "Down",  GP_DOWN,   false },
    { "Left",  GP_LEFT,   false },
    { "Right", GP_RIGHT,  false },
    { "A",     GP_A,      false },
    { "B",     GP_B,      false },
    { "C",     GP_RB,     false },
    { "X",     GP_X,      true  },
    { "Y",     GP_Y,      true  },
    { "Z",     GP_LB,     true  },
    { "Start", GP_START,  false },
    { "Mode",  GP_SELECT, true  },
};

PadKeyState pad_test_key_state(const PadTestKey &k, unsigned buttons, bool six_button)
{
    if (k.six_only && !six_button)
        return PadKeyState::Unavailable;
    return (buttons & k.bit) ? PadKeyState::Pressed : PadKeyState::Released;
}

void hold_exit_begin(HoldExit &h, bool held_now)
{
    h.armed    = !held_now;
    h.holding  = false;
    h.start_us = 0;
}

unsigned hold_exit_update(HoldExit &h, bool held, unsigned now_us,
                          unsigned hold_us, bool *exit)
{
    *exit = false;
    if (!held)
    {
        h.armed   = true;
        h.holding = false;
        return 0;
    }
    if (!h.armed)
        return 0;
    if (!h.holding)
    {
        h.holding  = true;
        h.start_us = now_us;
        return 0;
    }

    unsigned elapsed = now_us - h.start_us;   // wraps correctly (unsigned)
    if (elapsed >= hold_us)
    {
        *exit = true;
        return 100;
    }
    unsigned step = hold_us / 100u;           // avoids 64-bit math on ARM32
    if (step == 0) step = 1;
    unsigned pct = elapsed / step;
    return pct > 99 ? 99 : pct;
}

void pad_test_vid_pid(char *out, size_t n, unsigned vid, unsigned pid)
{
    static const char hx[] = "0123456789abcdef";
    char tmp[10];
    for (int i = 0; i < 4; i++) tmp[i]     = hx[(vid >> (12 - 4 * i)) & 0xF];
    tmp[4] = ':';
    for (int i = 0; i < 4; i++) tmp[5 + i] = hx[(pid >> (12 - 4 * i)) & 0xF];
    tmp[9] = '\0';

    if (n == 0) return;
    size_t i = 0;
    for (; i + 1 < n && tmp[i] != '\0'; i++) out[i] = tmp[i];
    out[i] = '\0';
}
