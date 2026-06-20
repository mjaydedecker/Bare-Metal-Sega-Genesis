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

// Face/shoulder buttons: RAW HID button bits as reported by Circle's generic
// gamepad driver (these are this controller's layout; observed on an 8BitDo).
// A generic auto-mapping is a deferred enhancement.
#define GP_A      (1u << 0)    // 0x001
#define GP_B      (1u << 1)    // 0x002
#define GP_X      (1u << 2)    // 0x004
#define GP_Y      (1u << 3)    // 0x008
#define GP_LB     (1u << 4)    // 0x010
#define GP_RB     (1u << 5)    // 0x020
#define GP_SELECT (1u << 8)    // 0x100
#define GP_START  (1u << 9)    // 0x200

// D-pad direction bits, synthesized from the HID hat in gamepad.cpp. Chosen at
// bits 15..18 (== Circle's normalized GamePadButtonUp.. ) so they (a) don't
// collide with the raw button bits above and (b) also match controllers handled
// by Circle's specific drivers.
#define GP_UP     (1u << 15)
#define GP_RIGHT  (1u << 16)
#define GP_DOWN   (1u << 17)
#define GP_LEFT   (1u << 18)

// buttons: TGamePadButton bitmask. retro_id: RETRO_DEVICE_ID_JOYPAD_*.
// Returns 1 if the mapped button is pressed, else 0.
int16_t joypad_state(unsigned buttons, unsigned retro_id);

#endif
