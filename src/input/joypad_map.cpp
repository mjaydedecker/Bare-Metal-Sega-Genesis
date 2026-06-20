//
// src/input/joypad_map.cpp
//
// Bare Metal Sega Genesis
// See joypad_map.h.
//

#include "joypad_map.h"

int16_t joypad_state(unsigned buttons, unsigned retro_id)
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
            if (joypad_state(buttons, ids[i]))
            {
                ret |= (int16_t) (1 << ids[i]);
            }
        }
        return ret;
    }

    unsigned mask;
    switch (retro_id)
    {
    case RETRO_DEVICE_ID_JOYPAD_UP:     mask = GP_UP;     break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:   mask = GP_DOWN;   break;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:   mask = GP_LEFT;   break;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT:  mask = GP_RIGHT;  break;
    case RETRO_DEVICE_ID_JOYPAD_A:      mask = GP_A;      break;
    case RETRO_DEVICE_ID_JOYPAD_B:      mask = GP_B;      break;
    case RETRO_DEVICE_ID_JOYPAD_X:      mask = GP_X;      break;
    case RETRO_DEVICE_ID_JOYPAD_Y:      mask = GP_Y;      break;
    case RETRO_DEVICE_ID_JOYPAD_L:      mask = GP_LB;     break;
    case RETRO_DEVICE_ID_JOYPAD_R:      mask = GP_RB;     break;
    case RETRO_DEVICE_ID_JOYPAD_START:  mask = GP_START;  break;
    case RETRO_DEVICE_ID_JOYPAD_SELECT: mask = GP_SELECT; break;
    default:                            return 0;
    }
    return (buttons & mask) ? 1 : 0;
}
