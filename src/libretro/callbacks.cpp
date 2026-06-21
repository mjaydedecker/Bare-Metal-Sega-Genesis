//
// src/libretro/callbacks.cpp
//
// Bare Metal Sega Genesis
// Stub libretro A/V and input callbacks.
//
// M4: all callbacks are no-ops so the core can be initialised and a ROM
//     loaded without crashing.  Real implementations come in M5–M7.
//

#include "callbacks.h"
#include "../video/display.h"
#include "../audio/audio_driver.h"
#include "../input/gamepad.h"
#include "../input/joypad_map.h"

Display *g_display = 0;
AudioDriver *g_audio = 0;
Gamepad *g_gamepad = 0;
const ButtonMap *g_map0 = 0;
const ButtonMap *g_map1 = 0;

void video_refresh_cb(const void *data, unsigned width, unsigned height,
                      size_t pitch)
{
    if (g_display != 0)
    {
        g_display->Blit(data, width, height, pitch);
    }
}

void audio_sample_cb(int16_t left, int16_t right)
{
    if (g_audio != 0)
    {
        int16_t frame[2] = { left, right };
        g_audio->Write(frame, 1);
    }
}

size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    if (g_audio != 0)
    {
        g_audio->Write(data, (unsigned) frames);
    }
    return frames;
}

void input_poll_cb(void)
{
    if (g_gamepad != 0)
    {
        g_gamepad->Poll();
    }
}

int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                       unsigned id)
{
    (void)index;
    if (port >= Gamepad::MAX_PADS || device != RETRO_DEVICE_JOYPAD ||
        g_gamepad == 0)
    {
        return 0;
    }
    const ButtonMap *map = (port == 0) ? g_map0 : g_map1;
    if (map == 0)
    {
        return 0;
    }
    unsigned buttons = g_gamepad->Buttons(port);
    if (port == 0 && (buttons & GP_SELECT))   // player-1 hotkey mode: mask input
    {
        return 0;
    }
    return joypad_state(buttons, id, *map);
}
