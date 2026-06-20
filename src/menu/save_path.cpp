//
// src/menu/save_path.cpp
//
// Bare Metal Sega Genesis
// See save_path.h.
//

#include "save_path.h"

static void append(char *out, unsigned out_size, unsigned *pos, const char *s)
{
    unsigned p = *pos;
    for (unsigned i = 0; s[i] != '\0' && p + 1 < out_size; i++) out[p++] = s[i];
    *pos = p;
}

void state_path(const char *romPath, int slot, char *out, unsigned out_size)
{
    if (out_size == 0) return;

    const char *base = romPath;            // basename: after the last '/'
    for (unsigned i = 0; romPath[i] != '\0'; i++)
    {
        if (romPath[i] == '/') base = &romPath[i + 1];
    }

    int s = slot;
    if (s < 0) s = 0;
    if (s > 9) s = 9;
    char digit[2] = { (char) ('0' + s), '\0' };

    unsigned pos = 0;
    append(out, out_size, &pos, "SD:/saves/");
    append(out, out_size, &pos, base);
    append(out, out_size, &pos, ".state");
    append(out, out_size, &pos, digit);
    out[pos] = '\0';
}
