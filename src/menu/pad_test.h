//
// src/menu/pad_test.h
//
// Bare Metal Sega Genesis
// Pure logic for the Controller Test screen: the Genesis button table (on the
// GP_* bits both sega_decode and USB pads report), per-button display state,
// hold-Start-to-exit timing, and the VID:PID label. No Circle deps.
//

#ifndef _menu_pad_test_h
#define _menu_pad_test_h

#include <stddef.h>

struct PadTestKey
{
    const char *label;
    unsigned    bit;        // GP_* bit
    bool        six_only;   // only exists on a 6-button pad (X/Y/Z/Mode)
};

// Index of each key in PAD_TEST_KEYS (the screen lays buttons out by these).
enum PadTestKeyIndex
{
    PTK_UP = 0, PTK_DOWN, PTK_LEFT, PTK_RIGHT,
    PTK_A, PTK_B, PTK_C, PTK_X, PTK_Y, PTK_Z, PTK_START, PTK_MODE
};

static const unsigned PAD_TEST_NUM_KEYS = 12;
extern const PadTestKey PAD_TEST_KEYS[PAD_TEST_NUM_KEYS];

enum class PadKeyState { Unavailable, Released, Pressed };

// six_button: the pad has X/Y/Z/Mode (6-button Sega pad, or any USB pad).
PadKeyState pad_test_key_state(const PadTestKey &k, unsigned buttons, bool six_button);

// Hold Start to leave the screen. If Start is already held when the screen
// opens (it was just used to confirm), it must be released once before a hold
// counts.
struct HoldExit
{
    bool     armed;
    bool     holding;
    unsigned start_us;
};

void hold_exit_begin(HoldExit &h, bool held_now);

// Feed the current Start state each loop. Returns hold progress 0..100 and sets
// *exit to true once Start has been held for hold_us.
unsigned hold_exit_update(HoldExit &h, bool held, unsigned now_us,
                          unsigned hold_us, bool *exit);

// "vvvv:pppp" in lowercase hex, truncated to fit out (always NUL-terminated).
void pad_test_vid_pid(char *out, size_t n, unsigned vid, unsigned pid);

#endif
