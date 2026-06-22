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

// --- cell builder helpers (bounded, NUL-terminated, no snprintf) ---

static void cell_uint(char *dst, unsigned v, unsigned cap)
{
    char tmp[10];
    int  n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char) ('0' + v % 10); v /= 10; }
    unsigned i = 0;
    while (n > 0 && i < cap) dst[i++] = tmp[--n];
    dst[i] = '\0';
}

static void cell_str(char *dst, const char *src, unsigned cap)
{
    unsigned i = 0;
    if (src) for (; src[i] && i < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static void cell_append(char *dst, unsigned *len, const char *src, unsigned cap)
{
    if (src) while (*src && *len < cap) dst[(*len)++] = *src++;
    dst[*len] = '\0';
}

static void cell_append_uint(char *dst, unsigned *len, unsigned v, unsigned cap)
{
    char tmp[10];
    int  n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char) ('0' + v % 10); v /= 10; }
    while (n > 0 && *len < cap) dst[(*len)++] = tmp[--n];
    dst[*len] = '\0';
}

static HudHealth fps_health(unsigned fps)
{
    if (fps >= 58) return HUD_GOOD;
    if (fps >= 50) return HUD_WARN;
    return HUD_BAD;
}
static HudHealth count_health(unsigned n) { return n == 0 ? HUD_GOOD : HUD_BAD; }

static void set_cell(HudCell *c, const char *label, HudHealth h)
{
    cell_str(c->label, label, HUD_LABEL_MAX);
    c->value[0] = '\0';
    c->health   = h;
}

unsigned hud_build(const HudStats &s, HudCell cells[], unsigned max)
{
    unsigned k = 0;
    unsigned len;

    if (k >= max) return k;                                  // FPS
    set_cell(&cells[k], "FPS", fps_health(s.fps));
    cell_uint(cells[k].value, s.fps, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // AQ queued/target
    set_cell(&cells[k], "AQ", HUD_INFO);
    len = 0;
    cell_append_uint(cells[k].value, &len, s.queued, HUD_VALUE_MAX);
    cell_append(cells[k].value, &len, "/", HUD_VALUE_MAX);
    cell_append_uint(cells[k].value, &len, s.target, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // UR
    set_cell(&cells[k], "UR", count_health(s.underruns));
    cell_uint(cells[k].value, s.underruns, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // OR
    set_cell(&cells[k], "OR", count_health(s.overruns));
    cell_uint(cells[k].value, s.overruns, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // ROM base name
    set_cell(&cells[k], "ROM", HUD_INFO);
    cell_str(cells[k].value, base_name(s.rom), HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // MODE = mode scale
    set_cell(&cells[k], "MODE", HUD_INFO);
    len = 0;
    if (s.mode) cell_append(cells[k].value, &len, s.mode, HUD_VALUE_MAX);
    cell_append(cells[k].value, &len, " ", HUD_VALUE_MAX);
    if (s.scale) cell_append(cells[k].value, &len, s.scale, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // VSYNC
    set_cell(&cells[k], "VSYNC", HUD_INFO);
    cell_str(cells[k].value, s.vsync ? "on" : "off", HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // WIDE
    set_cell(&cells[k], "WIDE", HUD_INFO);
    cell_str(cells[k].value, s.widescreen ? "on" : "off", HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // LAT
    set_cell(&cells[k], "LAT", HUD_INFO);
    cell_str(cells[k].value, s.latency ? s.latency : "-", HUD_VALUE_MAX);
    k++;

    return k;
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
