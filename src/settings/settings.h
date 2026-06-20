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

struct Settings
{
    ScaleMode scale_mode;   // video_scale: integer | stretch
    bool      widescreen;   // widescreen:  on | off

    Settings(void) : scale_mode(ScaleMode::Integer), widescreen(false) {}
};

// Parse key=value text into a Settings. Missing/invalid/unknown keys fall back
// to defaults. '#' lines and blank lines are ignored. Never fails; text may be
// NULL (yields all defaults).
Settings parse_settings(const char *text);

// Render a Settings to key=value text (NUL-terminated, truncated to out_size).
void serialize_settings(const Settings &s, char *out, size_t out_size);

#endif
