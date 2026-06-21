//
// src/input/hotkey.h
//
// Bare Metal Sega Genesis
// Pure decoder: maps a (held, pressed) GP_* bitmask pair to an in-game action
// using the user's hotkey bindings. No Circle dependency.
//

#ifndef _input_hotkey_h
#define _input_hotkey_h

#include "../settings/settings.h"   // HotkeyBindings, HK_*, MenuHotkey

enum class InGameAction
{
    None, QuickSave, QuickLoad, VolUp, VolDown, ToggleHud, Mute
};

// Returns the bound action when its hold is in `held` and its trigger is freshly
// in `pressed`; ties resolve by HK_ index order (lower wins). Else None.
InGameAction decode_hotkey(unsigned held, unsigned pressed,
                           const HotkeyBindings &hk);

// OR of pad_bit() of every binding's hold button (for input suppression).
unsigned hotkey_hold_mask(const HotkeyBindings &hk);

// Bit i set if binding i's combo duplicates another binding's, or equals the
// given menu-hotkey preset mask.
unsigned hotkey_conflicts(const HotkeyBindings &hk, MenuHotkey menu);

#endif
