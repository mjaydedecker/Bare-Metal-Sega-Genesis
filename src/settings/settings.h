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
enum class MenuHotkey { StartSelect, StartA, StartB, LR };
enum class PadButton { A, B, X, Y, L, R, Start, Select };
enum class VideoMode { Native, P1080, P720, P480 };
enum class AudioOutput { HDMI, Analog };

// Physical pad button driving each Genesis button, indexed
// 0=A,1=B,2=C,3=X,4=Y,5=Z,6=Start,7=Mode. Default ctor reproduces the
// historical mapping (Genesis A<-Y, B<-B, C<-A, X<-L, Y<-X, Z<-R,
// Start<-Start, Mode<-Select).
struct ButtonMap
{
    PadButton b[8];
    ButtonMap(void)
    {
        b[0] = PadButton::Y; b[1] = PadButton::B;
        b[2] = PadButton::A; b[3] = PadButton::L;
        b[4] = PadButton::X; b[5] = PadButton::R;
        b[6] = PadButton::Start; b[7] = PadButton::Select;
    }
};

struct Settings
{
    ScaleMode scale_mode;   // video_scale: integer | stretch
    bool      widescreen;   // widescreen:  on | off
    unsigned  volume;       // 0-100 master volume
    bool      mute;         // audio mute
    Region    region;       // region: auto | ntsc | pal
    char      auto_launch_rom[256];   // ROM path to boot into ("" = unset)
    MenuHotkey menu_hotkey; // controller combo that opens the pause menu
    ButtonMap  map1;        // Player 1 button map
    ButtonMap  map2;        // Player 2 button map
    VideoMode  video_mode;  // HDMI output mode
    AudioOutput audio_output;  // hdmi | analog (3.5mm jack)

    Settings(void)
    :   scale_mode(ScaleMode::Integer), widescreen(false),
        volume(100), mute(false), region(Region::Auto),
        menu_hotkey(MenuHotkey::StartSelect), video_mode(VideoMode::Native),
        audio_output(AudioOutput::HDMI)
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

// Menu hotkey as written to the settings file
// ("start+select" | "start+a" | "start+b" | "l+r").
const char *menu_hotkey_file_value(MenuHotkey h);

// Physical button token for the settings file ("a"|"b"|"x"|"y"|"l"|"r"|
// "start"|"select").
const char *pad_button_token(PadButton p);

// Parse a comma-encoded 8-token button map ("a,b,x,y,l,r,start,select" in
// Genesis order). Any bad token or wrong count yields the default map.
ButtonMap parse_button_map(const char *val);

// HDMI output dimensions for a VideoMode (Native => 0,0 = firmware's mode).
void video_mode_dims(VideoMode m, unsigned &w, unsigned &h);

// VideoMode as written to the settings file ("native"|"1080p"|"720p"|"480p").
const char *video_mode_file_value(VideoMode m);

// Audio output as written to the settings file ("hdmi" | "analog").
const char *audio_output_file_value(AudioOutput o);

#endif
