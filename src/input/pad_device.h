//
// src/input/pad_device.h
//
// Bare Metal Sega Genesis
// Pure choice of MD pad type (3- vs 6-button) for one port: live GPIO detection
// wins; with no usable GPIO pad it falls back to the global pad_type setting
// (which is what USB pads use). Kept free of libretro/Circle so it is
// host-testable; the kernel maps the bool to the RETRO_DEVICE_MDPAD_* constant.
//

#ifndef _input_pad_device_h
#define _input_pad_device_h

#include "sega_pad.h"               // SegaPadType
#include "../settings/settings.h"   // PadType

// True if the port should present a 6-button MD pad to the core.
bool pad_use_6button(SegaPadType detected, PadType setting);

#endif
