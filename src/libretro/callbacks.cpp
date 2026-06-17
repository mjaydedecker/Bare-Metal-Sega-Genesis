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

Display *g_display = 0;

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
    (void)left; (void)right;
}

size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    (void)data;
    return frames;
}

void input_poll_cb(void)
{
}

int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                       unsigned id)
{
    (void)port; (void)device; (void)index; (void)id;
    return 0;
}
