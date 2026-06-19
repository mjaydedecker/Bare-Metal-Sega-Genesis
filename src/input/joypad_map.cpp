//
// src/input/joypad_map.cpp
//
// Bare Metal Sega Genesis
// See joypad_map.h.
//

#include "joypad_map.h"

int16_t joypad_state(unsigned buttons, unsigned retro_id)
{
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
