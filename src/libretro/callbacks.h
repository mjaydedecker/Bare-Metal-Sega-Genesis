//
// src/libretro/callbacks.h
//
// Bare Metal Sega Genesis
// Stub libretro A/V and input callbacks — wired up in M4, implemented
// properly in M5 (video), M6 (audio), and M7 (input).
//

#ifndef _libretro_callbacks_h
#define _libretro_callbacks_h

#include <libretro.h>

// Set by the kernel before the frame loop; video_refresh_cb forwards
// frames here. Forward-declared to avoid pulling Circle into this header.
class Display;
extern Display *g_display;

// Set by the kernel before the frame loop; the audio callbacks forward
// samples here. Forward-declared to avoid pulling Circle into this header.
class AudioDriver;
extern AudioDriver *g_audio;

// Set by the kernel before the frame loop; the input callbacks read the pad
// here. Forward-declared to avoid pulling Circle into this header.
class Gamepad;
extern Gamepad *g_gamepad;

// Per-port button maps (point into the kernel's Settings). input_state_cb reads
// them so the core sees the user's remapping.
struct ButtonMap;
extern const ButtonMap *g_map0;
extern const ButtonMap *g_map1;

// These four callbacks are passed to retro_set_video_refresh(),
// retro_set_audio_sample(), retro_set_audio_sample_batch(),
// retro_set_input_poll(), and retro_set_input_state() respectively.

void video_refresh_cb(const void *data, unsigned width, unsigned height,
                      size_t pitch);

void audio_sample_cb(int16_t left, int16_t right);

size_t audio_batch_cb(const int16_t *data, size_t frames);

void input_poll_cb(void);

int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                       unsigned id);

#endif
