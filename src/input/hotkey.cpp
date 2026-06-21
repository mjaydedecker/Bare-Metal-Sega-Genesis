//
// src/input/hotkey.cpp
//
// Bare Metal Sega Genesis
// See hotkey.h.
//

#include "hotkey.h"
#include "joypad_map.h"   // pad_bit, hotkey_mask, GP_* bits

InGameAction decode_hotkey(unsigned held, unsigned pressed,
                           const HotkeyBindings &hk)
{
    for (int i = 0; i < HK_COUNT; i++)
    {
        if ((held & pad_bit(hk.b[i].hold)) &&
            (pressed & pad_bit(hk.b[i].trigger)))
        {
            return (InGameAction) (i + 1);   // HK_* index -> InGameAction value
        }
    }
    return InGameAction::None;
}

unsigned hotkey_hold_mask(const HotkeyBindings &hk)
{
    unsigned m = 0;
    for (int i = 0; i < HK_COUNT; i++) m |= pad_bit(hk.b[i].hold);
    return m;
}

unsigned hotkey_conflicts(const HotkeyBindings &hk, MenuHotkey menu)
{
    unsigned combo[HK_COUNT];
    for (int i = 0; i < HK_COUNT; i++)
        combo[i] = pad_bit(hk.b[i].hold) | pad_bit(hk.b[i].trigger);

    unsigned menuMask = hotkey_mask(menu);
    unsigned bits = 0;
    for (int i = 0; i < HK_COUNT; i++)
    {
        if (combo[i] == menuMask) bits |= (1u << i);
        for (int j = 0; j < HK_COUNT; j++)
            if (j != i && combo[i] == combo[j]) bits |= (1u << i);
    }
    return bits;
}
