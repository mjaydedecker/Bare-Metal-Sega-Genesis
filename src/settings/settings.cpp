//
// src/settings/settings.cpp
//
// Bare Metal Sega Genesis
// See settings.h.
//

#include "settings.h"
#include <string.h>

// Case-insensitive ASCII equality.
static bool ieq(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char) (ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char) (cb + 32);
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static bool truthy(const char *v)
{
    return ieq(v, "on") || ieq(v, "true") || ieq(v, "1") || ieq(v, "yes");
}

static bool parse_hotkey(const char *val, HotkeyBinding *out);   // defined below

Settings parse_settings(const char *text)
{
    Settings s;                       // defaults
    if (text == 0) return s;

    const char *p = text;
    while (*p)
    {
        // Copy one line into a local buffer.
        char line[128];
        size_t n = 0;
        while (*p && *p != '\n' && *p != '\r')
        {
            if (n < sizeof(line) - 1) line[n++] = *p;
            p++;
        }
        line[n] = '\0';
        while (*p == '\n' || *p == '\r') p++;   // consume EOL(s)

        // Trim leading blanks; skip comments / empty lines.
        char *l = line;
        while (*l == ' ' || *l == '\t') l++;
        if (*l == '#' || *l == '\0') continue;

        // Split on '='.
        char *eq = strchr(l, '=');
        if (eq == 0) continue;
        *eq = '\0';
        char *key = l;
        char *val = eq + 1;

        // Trim trailing blanks on key.
        char *ke = key + strlen(key);
        while (ke > key && (ke[-1] == ' ' || ke[-1] == '\t')) *--ke = '\0';
        // Trim leading + trailing blanks on value.
        while (*val == ' ' || *val == '\t') val++;
        char *ve = val + strlen(val);
        while (ve > val && (ve[-1] == ' ' || ve[-1] == '\t')) *--ve = '\0';

        if (ieq(key, "video_scale"))
            s.scale_mode = ieq(val, "stretch") ? ScaleMode::Stretch
                         : ieq(val, "aspect")  ? ScaleMode::Aspect
                                               : ScaleMode::Integer;
        else if (ieq(key, "widescreen"))
            s.widescreen = truthy(val);
        else if (ieq(key, "volume"))
        {
            unsigned v = 0;
            bool any = false;
            for (const char *q = val; *q >= '0' && *q <= '9'; q++)
            {
                v = v * 10 + (unsigned) (*q - '0');
                any = true;
                if (v > 100000) break;        // guard against silly input
            }
            if (any)
            {
                if (v > 100) v = 100;
                s.volume = v;
            }
            // non-numeric: keep default
        }
        else if (ieq(key, "mute"))
            s.mute = truthy(val);
        else if (ieq(key, "region"))
        {
            if      (ieq(val, "ntsc")) s.region = Region::NTSC;
            else if (ieq(val, "pal"))  s.region = Region::PAL;
            else                       s.region = Region::Auto;
        }
        else if (ieq(key, "auto_launch_rom"))
        {
            unsigned i = 0;
            for (; val[i] && i < sizeof(s.auto_launch_rom) - 1; i++)
                s.auto_launch_rom[i] = val[i];
            s.auto_launch_rom[i] = '\0';
        }
        else if (ieq(key, "menu_hotkey"))
        {
            if      (ieq(val, "start+a")) s.menu_hotkey = MenuHotkey::StartA;
            else if (ieq(val, "start+b")) s.menu_hotkey = MenuHotkey::StartB;
            else if (ieq(val, "l+r"))     s.menu_hotkey = MenuHotkey::LR;
            else                          s.menu_hotkey = MenuHotkey::StartSelect;
        }
        else if (ieq(key, "pad_type"))
            s.pad_type = ieq(val, "3button") ? PadType::ThreeButton
                                              : PadType::SixButton;
        else if (ieq(key, "controller_1_map"))
            s.map1 = parse_button_map(val);
        else if (ieq(key, "controller_2_map"))
            s.map2 = parse_button_map(val);
        else if (ieq(key, "video_mode"))
        {
            if      (ieq(val, "1080p")) s.video_mode = VideoMode::P1080;
            else if (ieq(val, "720p"))  s.video_mode = VideoMode::P720;
            else if (ieq(val, "480p"))  s.video_mode = VideoMode::P480;
            else                        s.video_mode = VideoMode::Native;
        }
        else if (ieq(key, "audio_output"))
        {
            if      (ieq(val, "analog")) s.audio_output = AudioOutput::Analog;
            else if (ieq(val, "i2s"))    s.audio_output = AudioOutput::I2S;
            else                         s.audio_output = AudioOutput::HDMI;
        }
        else if (ieq(key, "audio_latency"))
        {
            if      (ieq(val, "low"))  s.audio_latency = AudioLatency::Low;
            else if (ieq(val, "high")) s.audio_latency = AudioLatency::High;
            else                       s.audio_latency = AudioLatency::Medium;
        }
        else if (ieq(key, "vsync"))
            s.vsync = truthy(val);
        else if (ieq(key, "debug_overlay"))
            s.debug_overlay = truthy(val);
        else if (ieq(key, "hotkey_quicksave")) parse_hotkey(val, &s.hotkeys.b[HK_QUICKSAVE]);
        else if (ieq(key, "hotkey_quickload")) parse_hotkey(val, &s.hotkeys.b[HK_QUICKLOAD]);
        else if (ieq(key, "hotkey_volup"))     parse_hotkey(val, &s.hotkeys.b[HK_VOLUP]);
        else if (ieq(key, "hotkey_voldown"))   parse_hotkey(val, &s.hotkeys.b[HK_VOLDOWN]);
        else if (ieq(key, "hotkey_togglehud")) parse_hotkey(val, &s.hotkeys.b[HK_TOGGLEHUD]);
        else if (ieq(key, "hotkey_mute"))      parse_hotkey(val, &s.hotkeys.b[HK_MUTE]);
        // unknown keys: ignored
    }
    return s;
}

// Append src to out without overflowing out_size (out must be NUL-terminated).
static void appendz(char *out, size_t out_size, const char *src)
{
    size_t len = strlen(out);
    size_t i   = 0;
    while (src[i] && len + 1 < out_size) out[len++] = src[i++];
    out[len] = '\0';
}

// Append an unsigned integer as decimal text.
static void append_uint(char *out, size_t out_size, unsigned v)
{
    char rev[12];
    int  n = 0;
    if (v == 0) rev[n++] = '0';
    else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
    char fwd[12];
    int  m = 0;
    while (n) fwd[m++] = rev[--n];
    fwd[m] = '\0';
    appendz(out, out_size, fwd);
}

const char *region_file_value(Region r)
{
    switch (r)
    {
    case Region::NTSC: return "ntsc";
    case Region::PAL:  return "pal";
    default:           return "auto";
    }
}

const char *region_core_value(Region r)
{
    switch (r)
    {
    case Region::NTSC: return "ntsc-u";
    case Region::PAL:  return "pal";
    default:           return "auto";
    }
}

const char *menu_hotkey_file_value(MenuHotkey h)
{
    switch (h)
    {
    case MenuHotkey::StartA: return "start+a";
    case MenuHotkey::StartB: return "start+b";
    case MenuHotkey::LR:     return "l+r";
    default:                 return "start+select";
    }
}

const char *pad_type_file_value(PadType p)
{
    return p == PadType::ThreeButton ? "3button" : "6button";
}

const char *pad_button_token(PadButton p)
{
    switch (p)
    {
    case PadButton::A:      return "a";
    case PadButton::B:      return "b";
    case PadButton::X:      return "x";
    case PadButton::Y:      return "y";
    case PadButton::L:      return "l";
    case PadButton::R:      return "r";
    case PadButton::Start:  return "start";
    case PadButton::Select: return "select";
    }
    return "a";
}

static int pad_button_from_token(const char *t)
{
    if (ieq(t, "a"))      return (int) PadButton::A;
    if (ieq(t, "b"))      return (int) PadButton::B;
    if (ieq(t, "x"))      return (int) PadButton::X;
    if (ieq(t, "y"))      return (int) PadButton::Y;
    if (ieq(t, "l"))      return (int) PadButton::L;
    if (ieq(t, "r"))      return (int) PadButton::R;
    if (ieq(t, "start"))  return (int) PadButton::Start;
    if (ieq(t, "select")) return (int) PadButton::Select;
    return -1;
}

// Parse "hold+trigger" (e.g. "select+x") into out. Returns false (caller keeps
// the default) on a bad token, a hold outside {L,R,Start,Select}, or hold==trigger.
static bool parse_hotkey(const char *val, HotkeyBinding *out)
{
    char a[16], b[16];
    int  i = 0;
    const char *p = val;
    while (*p && *p != '+' && i < 15) a[i++] = *p++;
    a[i] = '\0';
    if (*p != '+') return false;
    p++;
    int j = 0;
    while (*p && j < 15) b[j++] = *p++;
    b[j] = '\0';

    int h = pad_button_from_token(a);
    int t = pad_button_from_token(b);
    if (h < 0 || t < 0 || h == t) return false;
    if (h < (int) PadButton::L) return false;   // safe holds: L,R,Start,Select
    out->hold    = (PadButton) h;
    out->trigger = (PadButton) t;
    return true;
}

ButtonMap parse_button_map(const char *val)
{
    PadButton tmp[8];
    int n = 0;
    char tok[16];
    int ti = 0;
    for (const char *p = val; ; p++)
    {
        char c = *p;
        if (c == ',' || c == '\0')
        {
            tok[ti] = '\0';
            if (n >= 8) return ButtonMap();          // too many tokens
            int pb = pad_button_from_token(tok);
            if (pb < 0) return ButtonMap();          // bad token
            tmp[n++] = (PadButton) pb;
            ti = 0;
            if (c == '\0') break;
        }
        else if (ti < (int) sizeof(tok) - 1)
        {
            tok[ti++] = c;
        }
    }
    if (n != 8) return ButtonMap();                  // wrong count
    ButtonMap m;
    for (int i = 0; i < 8; i++) m.b[i] = tmp[i];
    return m;
}

// Append a button map as 8 comma-separated tokens.
static void append_map(char *out, size_t out_size, const ButtonMap &m)
{
    for (int i = 0; i < 8; i++)
    {
        if (i) appendz(out, out_size, ",");
        appendz(out, out_size, pad_button_token(m.b[i]));
    }
}

void video_mode_dims(VideoMode m, unsigned &w, unsigned &h)
{
    switch (m)
    {
    case VideoMode::P1080: w = 1920; h = 1080; break;
    case VideoMode::P720:  w = 1280; h = 720;  break;
    case VideoMode::P480:  w = 720;  h = 480;  break;
    default:               w = 0;    h = 0;    break;   // Native
    }
}

const char *video_mode_file_value(VideoMode m)
{
    switch (m)
    {
    case VideoMode::P1080: return "1080p";
    case VideoMode::P720:  return "720p";
    case VideoMode::P480:  return "480p";
    default:               return "native";
    }
}

const char *audio_output_file_value(AudioOutput o)
{
    switch (o)
    {
    case AudioOutput::Analog: return "analog";
    case AudioOutput::I2S:    return "i2s";
    default:                  return "hdmi";
    }
}

const char *audio_latency_file_value(AudioLatency l)
{
    switch (l)
    {
    case AudioLatency::Low:  return "low";
    case AudioLatency::High: return "high";
    default:                 return "medium";
    }
}

unsigned audio_latency_frames(AudioLatency l)
{
    switch (l)
    {
    case AudioLatency::Low:  return 1;
    case AudioLatency::High: return 3;
    default:                 return 2;   // Medium == today's behavior
    }
}

void serialize_settings(const Settings &s, char *out, size_t out_size)
{
    if (out == 0 || out_size == 0) return;
    out[0] = '\0';
    appendz(out, out_size, "# Bare Metal Sega Genesis settings\n");
    appendz(out, out_size, "video_scale=");
    appendz(out, out_size,
            s.scale_mode == ScaleMode::Stretch ? "stretch"
          : s.scale_mode == ScaleMode::Aspect  ? "aspect"
                                               : "integer");
    appendz(out, out_size, "\nwidescreen=");
    appendz(out, out_size, s.widescreen ? "on" : "off");
    appendz(out, out_size, "\nvolume=");
    append_uint(out, out_size, s.volume);
    appendz(out, out_size, "\nmute=");
    appendz(out, out_size, s.mute ? "on" : "off");
    appendz(out, out_size, "\nregion=");
    appendz(out, out_size, region_file_value(s.region));
    appendz(out, out_size, "\nauto_launch_rom=");
    appendz(out, out_size, s.auto_launch_rom);
    appendz(out, out_size, "\nmenu_hotkey=");
    appendz(out, out_size, menu_hotkey_file_value(s.menu_hotkey));
    appendz(out, out_size, "\npad_type=");
    appendz(out, out_size, pad_type_file_value(s.pad_type));
    appendz(out, out_size, "\ncontroller_1_map=");
    append_map(out, out_size, s.map1);
    appendz(out, out_size, "\ncontroller_2_map=");
    append_map(out, out_size, s.map2);
    appendz(out, out_size, "\nvideo_mode=");
    appendz(out, out_size, video_mode_file_value(s.video_mode));
    appendz(out, out_size, "\naudio_output=");
    appendz(out, out_size, audio_output_file_value(s.audio_output));
    appendz(out, out_size, "\naudio_latency=");
    appendz(out, out_size, audio_latency_file_value(s.audio_latency));
    appendz(out, out_size, "\nvsync=");
    appendz(out, out_size, s.vsync ? "on" : "off");
    appendz(out, out_size, "\ndebug_overlay=");
    appendz(out, out_size, s.debug_overlay ? "on" : "off");

    static const char *const HK_KEY[HK_COUNT] = {
        "hotkey_quicksave", "hotkey_quickload", "hotkey_volup",
        "hotkey_voldown", "hotkey_togglehud", "hotkey_mute" };
    for (int i = 0; i < HK_COUNT; i++)
    {
        appendz(out, out_size, "\n");
        appendz(out, out_size, HK_KEY[i]);
        appendz(out, out_size, "=");
        appendz(out, out_size, pad_button_token(s.hotkeys.b[i].hold));
        appendz(out, out_size, "+");
        appendz(out, out_size, pad_button_token(s.hotkeys.b[i].trigger));
    }
    appendz(out, out_size, "\n");
}
