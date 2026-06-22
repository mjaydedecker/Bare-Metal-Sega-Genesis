//
// src/input/controller_map.cpp
//
// Bare Metal Sega Genesis
// See controller_map.h.
//

#include "controller_map.h"
#include "joypad_map.h"   // pad_bit, PadButton

// Parse up to 4 hex digits at *p (advancing p); returns the value, sets ok.
static unsigned parse_hex(const char *&p, bool &ok)
{
    unsigned v = 0;
    int n = 0;
    while (*p)
    {
        char ch = *p;
        unsigned d;
        if      (ch >= '0' && ch <= '9') d = (unsigned)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') d = (unsigned)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') d = (unsigned)(ch - 'A' + 10);
        else break;
        v = v * 16 + d; n++; p++;
    }
    ok = (n > 0);
    return v;
}

// Parse a decimal at *p (advancing p); returns the value, sets ok.
static unsigned parse_dec(const char *&p, bool &ok)
{
    unsigned v = 0;
    int n = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (unsigned)(*p - '0'); n++; p++; }
    ok = (n > 0);
    return v;
}

bool controller_parse_line(const char *line, ControllerCal *out)
{
    if (line == 0) return false;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '#' || *line == '\0') return false;

    const char *p = line;
    bool ok = false;
    unsigned vid = parse_hex(p, ok);  if (!ok || *p != ':') return false;
    p++;
    unsigned pid = parse_hex(p, ok);  if (!ok || *p != '=') return false;
    p++;

    uint8_t bits[8];
    for (int i = 0; i < 8; i++)
    {
        unsigned b = parse_dec(p, ok);
        if (!ok) return false;
        if (b != 255 && b > 31) return false;
        bits[i] = (uint8_t) b;
        if (i < 7) { if (*p != ',') return false; p++; }
    }
    // trailing chars after the 8th value (other than whitespace) are rejected
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '\0') return false;

    out->vid = (uint16_t) vid;
    out->pid = (uint16_t) pid;
    for (int i = 0; i < 8; i++) out->bit[i] = bits[i];
    return true;
}

// Append a 4-digit lowercase hex of v to out[*len], bounded.
static void app_hex4(char *out, size_t *len, size_t cap, unsigned v)
{
    static const char hx[] = "0123456789abcdef";
    for (int shift = 12; shift >= 0; shift -= 4)
        if (*len + 1 < cap) out[(*len)++] = hx[(v >> shift) & 0xF];
    out[*len] = '\0';
}

// Append decimal v to out[*len], bounded.
static void app_dec(char *out, size_t *len, size_t cap, unsigned v)
{
    char tmp[4]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0 && *len + 1 < cap) out[(*len)++] = tmp[--n];
    out[*len] = '\0';
}

static void app_ch(char *out, size_t *len, size_t cap, char c)
{
    if (*len + 1 < cap) out[(*len)++] = c;
    out[*len] = '\0';
}

void controller_serialize_line(const ControllerCal &c, char *out, size_t out_size)
{
    if (out == 0 || out_size == 0) return;
    size_t len = 0; out[0] = '\0';
    app_hex4(out, &len, out_size, c.vid);
    app_ch(out, &len, out_size, ':');
    app_hex4(out, &len, out_size, c.pid);
    app_ch(out, &len, out_size, '=');
    for (int i = 0; i < 8; i++)
    {
        if (i) app_ch(out, &len, out_size, ',');
        app_dec(out, &len, out_size, c.bit[i]);
    }
}

unsigned controller_decode(unsigned raw_buttons, const ControllerCal &cal)
{
    unsigned m = 0;
    for (int i = 0; i < 8; i++)
    {
        uint8_t b = cal.bit[i];
        if (b != 255 && ((raw_buttons >> b) & 1u))
            m |= pad_bit((PadButton) i);
    }
    return m;
}

const ControllerCal *controller_find(const ControllerCal *arr, unsigned count,
                                     uint16_t vid, uint16_t pid)
{
    for (unsigned i = 0; i < count; i++)
        if (arr[i].vid == vid && arr[i].pid == pid) return &arr[i];
    return 0;
}
