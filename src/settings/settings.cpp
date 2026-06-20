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
    appendz(out, out_size, "\n");
}
