//
// src/settings/settings.h
//
// Bare Metal Sega Genesis
// Settings model: a typed struct plus pure parse/serialize over key=value text.
// No Circle dependencies — host-testable.
//

#ifndef _settings_settings_h
#define _settings_settings_h

#include <stddef.h>

enum class ScaleMode { Integer, Stretch };
enum class Region    { Auto, NTSC, PAL };

struct Settings
{
    ScaleMode scale_mode;   // video_scale: integer | stretch
    bool      widescreen;   // widescreen:  on | off
    unsigned  volume;       // 0-100 master volume
    bool      mute;         // audio mute
    Region    region;       // region: auto | ntsc | pal
    char      auto_launch_rom[256];   // ROM path to boot into ("" = unset)

    Settings(void)
    :   scale_mode(ScaleMode::Integer), widescreen(false),
        volume(100), mute(false), region(Region::Auto)
    {
        auto_launch_rom[0] = '\0';
    }
};

// Parse key=value text into a Settings. Missing/invalid/unknown keys fall back
// to defaults. '#' lines and blank lines are ignored. Never fails; text may be
// NULL (yields all defaults).
Settings parse_settings(const char *text);

// Render a Settings to key=value text (NUL-terminated, truncated to out_size).
void serialize_settings(const Settings &s, char *out, size_t out_size);

// Region as written to the settings file ("auto" | "ntsc" | "pal").
const char *region_file_value(Region r);

// Region as the core's option value ("auto" | "ntsc-u" | "pal").
const char *region_core_value(Region r);

#endif
