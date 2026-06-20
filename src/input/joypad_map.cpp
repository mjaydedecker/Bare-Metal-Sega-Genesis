//
// src/input/joypad_map.cpp
//
// Bare Metal Sega Genesis
// See joypad_map.h.
//

#include "joypad_map.h"

unsigned pad_bit(PadButton p)
{
    switch (p)
    {
    case PadButton::A:      return GP_A;
    case PadButton::B:      return GP_B;
    case PadButton::X:      return GP_X;
    case PadButton::Y:      return GP_Y;
    case PadButton::L:      return GP_LB;
    case PadButton::R:      return GP_RB;
    case PadButton::Start:  return GP_START;
    case PadButton::Select: return GP_SELECT;
    }
    return 0;
}

// libretro id for each Genesis button index (A,B,C,X,Y,Z,Start,Mode).
static const unsigned k_action_id[8] = {
    RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B,
    RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_L,
    RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_R,
    RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT
};

int16_t joypad_state(unsigned buttons, unsigned retro_id, const ButtonMap &map)
{
    // The core reads the pad in one call via RETRO_DEVICE_ID_JOYPAD_MASK,
    // expecting a bitmask where bit N = button id N pressed. Build it.
    if (retro_id == RETRO_DEVICE_ID_JOYPAD_MASK)
    {
        static const unsigned ids[] = {
            RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_DOWN,
            RETRO_DEVICE_ID_JOYPAD_LEFT,  RETRO_DEVICE_ID_JOYPAD_RIGHT,
            RETRO_DEVICE_ID_JOYPAD_A,     RETRO_DEVICE_ID_JOYPAD_B,
            RETRO_DEVICE_ID_JOYPAD_X,     RETRO_DEVICE_ID_JOYPAD_Y,
            RETRO_DEVICE_ID_JOYPAD_L,     RETRO_DEVICE_ID_JOYPAD_R,
            RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT
        };
        int16_t ret = 0;
        for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++)
        {
            if (joypad_state(buttons, ids[i], map))
            {
                ret |= (int16_t) (1 << ids[i]);
            }
        }
        return ret;
    }

    // D-pad: fixed directional mapping.
    unsigned mask = 0;
    switch (retro_id)
    {
    case RETRO_DEVICE_ID_JOYPAD_UP:    mask = GP_UP;    break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:  mask = GP_DOWN;  break;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:  mask = GP_LEFT;  break;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT: mask = GP_RIGHT; break;
    default: break;
    }
    if (mask) return (buttons & mask) ? 1 : 0;

    // Action buttons: remappable via the ButtonMap.
    for (int i = 0; i < 8; i++)
    {
        if (k_action_id[i] == retro_id)
        {
            return (buttons & pad_bit(map.b[i])) ? 1 : 0;
        }
    }

    return 0;
}

unsigned hotkey_mask(MenuHotkey h)
{
    switch (h)
    {
    case MenuHotkey::StartA: return GP_START | GP_A;
    case MenuHotkey::StartB: return GP_START | GP_B;
    case MenuHotkey::LR:     return GP_LB | GP_RB;
    default:                 return GP_START | GP_SELECT;
    }
}
