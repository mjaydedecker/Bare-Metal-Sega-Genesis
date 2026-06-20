//
// src/menu/rom_filter.cpp
//
// Bare Metal Sega Genesis
// See rom_filter.h.
//

#include "rom_filter.h"
#include <string.h>

static bool ends_with_ci(const char *s, const char *suffix)
{
    size_t ls = strlen(s);
    size_t lf = strlen(suffix);
    if (lf == 0 || ls <= lf)        // need at least one char before the suffix
        return false;
    const char *p = s + (ls - lf);
    for (size_t i = 0; i < lf; i++)
    {
        char a = p[i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return false;
    }
    return true;
}

bool is_rom_filename(const char *name)
{
    if (name == 0) return false;
    return ends_with_ci(name, ".md")
        || ends_with_ci(name, ".bin")
        || ends_with_ci(name, ".gen");
}
