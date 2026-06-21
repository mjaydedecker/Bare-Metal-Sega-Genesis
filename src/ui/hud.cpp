//
// src/ui/hud.cpp
//
// Bare Metal Sega Genesis
// See hud.h. Manual formatting only (no snprintf) so it links on bare metal.
//

#include "hud.h"

// Append decimal v to line[*len], bounded by HUD_COLS. Keeps line NUL-terminated.
static void app_uint(char *line, unsigned *len, unsigned v)
{
    char tmp[10];
    int  n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char) ('0' + v % 10); v /= 10; }
    while (n > 0 && *len < HUD_COLS) line[(*len)++] = tmp[--n];
    line[*len] = '\0';
}

// Append string src to line[*len], bounded by HUD_COLS.
static void app_str(char *line, unsigned *len, const char *src)
{
    while (*src && *len < HUD_COLS) line[(*len)++] = *src++;
    line[*len] = '\0';
}

// Pointer to the char after the last '/' or ':' in p ("-" if p is NULL).
static const char *base_name(const char *p)
{
    if (p == 0) return "-";
    const char *b = p;
    for (const char *q = p; *q; q++)
        if (*q == '/' || *q == ':') b = q + 1;
    return b;
}

unsigned hud_format(const HudStats &s, char lines[][HUD_COLS + 1],
                    unsigned max_lines)
{
    unsigned count = 0;
    unsigned len;

    if (count >= max_lines) return count;          // line 0: FPS / U / O
    len = 0; lines[count][0] = '\0';
    app_str(lines[count], &len, "FPS ");  app_uint(lines[count], &len, s.fps);
    app_str(lines[count], &len, "  U ");  app_uint(lines[count], &len, s.underruns);
    app_str(lines[count], &len, "  O ");  app_uint(lines[count], &len, s.overruns);
    count++;

    if (count >= max_lines) return count;          // line 1: AQ queued/target
    len = 0; lines[count][0] = '\0';
    app_str(lines[count], &len, "AQ ");   app_uint(lines[count], &len, s.queued);
    app_str(lines[count], &len, "/");     app_uint(lines[count], &len, s.target);
    count++;

    if (count >= max_lines) return count;          // line 2: ROM base name
    len = 0; lines[count][0] = '\0';
    app_str(lines[count], &len, base_name(s.rom));
    count++;

    if (count >= max_lines) return count;          // line 3: mode  scale
    len = 0; lines[count][0] = '\0';
    if (s.mode)  app_str(lines[count], &len, s.mode);
    app_str(lines[count], &len, "  ");
    if (s.scale) app_str(lines[count], &len, s.scale);
    count++;

    return count;
}
