//
// src/input/hotkey.cpp
//
// Bare Metal Sega Genesis
// See hotkey.h.
//

#include "hotkey.h"
#include "joypad_map.h"   // GP_* bits

InGameAction decode_hotkey(unsigned held, unsigned pressed)
{
    if (!(held & GP_SELECT)) return InGameAction::None;   // modifier required

    if (pressed & GP_X)    return InGameAction::QuickSave;
    if (pressed & GP_Y)    return InGameAction::QuickLoad;
    if (pressed & GP_A)    return InGameAction::ToggleHud;
    if (pressed & GP_B)    return InGameAction::Mute;
    if (pressed & GP_UP)   return InGameAction::VolUp;
    if (pressed & GP_DOWN) return InGameAction::VolDown;
    return InGameAction::None;
}
