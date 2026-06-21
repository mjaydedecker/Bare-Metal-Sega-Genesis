//
// src/input/hotkey.h
//
// Bare Metal Sega Genesis
// Pure decoder: maps a (held, pressed) GP_* bitmask pair to an in-game action
// for the fixed Select+button hotkey scheme. No Circle dependency.
//

#ifndef _input_hotkey_h
#define _input_hotkey_h

enum class InGameAction
{
    None, QuickSave, QuickLoad, VolUp, VolDown, ToggleHud, Mute
};

// held    = current GP_* bitmask this frame.
// pressed = newly-pressed edge bits this frame.
// Returns the mapped action when Select is held and the action button is freshly
// pressed; otherwise None.
InGameAction decode_hotkey(unsigned held, unsigned pressed);

#endif
