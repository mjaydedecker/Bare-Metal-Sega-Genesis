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

void serialize_settings(const Settings &s, char *out, size_t out_size)
{
    if (out == 0 || out_size == 0) return;
    out[0] = '\0';
    appendz(out, out_size, "# Bare Metal Sega Genesis settings\n");
    appendz(out, out_size, "video_scale=");
    appendz(out, out_size, s.scale_mode == ScaleMode::Stretch ? "stretch"
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
    appendz(out, out_size, "\n");
}
