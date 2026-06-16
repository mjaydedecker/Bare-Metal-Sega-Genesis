//
// src/libretro/environment.h
//
// Bare Metal Sega Genesis
// libretro environment callback — handles core queries during
// retro_set_environment() / retro_init() / retro_load_game().
//

#ifndef _libretro_environment_h
#define _libretro_environment_h

#include <libretro.h>

// Pass to retro_set_environment() before calling retro_init().
bool environment_cb(unsigned cmd, void *data);

// Pixel format chosen by the core (set via RETRO_ENVIRONMENT_SET_PIXEL_FORMAT).
// Defaults to RGB1555 (libretro's legacy default).
extern retro_pixel_format g_pixel_format;

// Set before calling retro_load_game() so the environment callback can
// serve RETRO_ENVIRONMENT_GET_GAME_INFO_EXT with the in-memory ROM.
extern const void *g_rom_data;
extern size_t      g_rom_size;

#endif
