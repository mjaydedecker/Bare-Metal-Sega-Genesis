//
// src/input/joypad_map.h
//
// Bare Metal Sega Genesis
// Pure libretro-joypad-id -> gamepad-button mapping. No Circle deps so it is
// host-testable. The GP_* bits mirror Circle's TGamePadButton (verified by a
// static_assert in gamepad.cpp).
//

#ifndef _input_joypad_map_h
#define _input_joypad_map_h

#include <stdint.h>
#include <libretro.h>   // RETRO_DEVICE_ID_JOYPAD_*

#define GP_Y      (1u << 7)
#define GP_B      (1u << 8)
#define GP_A      (1u << 9)
#define GP_X      (1u << 10)
#define GP_LB     (1u << 5)
#define GP_RB     (1u << 6)
#define GP_SELECT (1u << 11)
#define GP_START  (1u << 14)
#define GP_UP     (1u << 15)
#define GP_RIGHT  (1u << 16)
#define GP_DOWN   (1u << 17)
#define GP_LEFT   (1u << 18)

// buttons: TGamePadButton bitmask. retro_id: RETRO_DEVICE_ID_JOYPAD_*.
// Returns 1 if the mapped button is pressed, else 0.
int16_t joypad_state(unsigned buttons, unsigned retro_id);

#endif
