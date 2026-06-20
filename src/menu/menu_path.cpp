//
// src/menu/menu_path.cpp
//
// Bare Metal Sega Genesis
// See menu_path.h.
//

#include "menu_path.h"
#include <string.h>

static void copy_bounded(char *out, unsigned out_size, const char *src)
{
    if (out_size == 0) return;
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < out_size; i++) out[i] = src[i];
    out[i] = '\0';
}

void path_join(const char *base, const char *name, char *out, unsigned out_size)
{
    char tmp[512];                       // build into a temp so out may alias base
    unsigned n = 0;
    for (unsigned i = 0; base[i] != '\0' && n + 1 < sizeof tmp; i++) tmp[n++] = base[i];
    if (n > 0 && tmp[n - 1] != '/' && n + 1 < sizeof tmp) tmp[n++] = '/';
    for (unsigned i = 0; name[i] != '\0' && n + 1 < sizeof tmp; i++) tmp[n++] = name[i];
    tmp[n] = '\0';
    copy_bounded(out, out_size, tmp);
}

void path_parent(const char *path, const char *root, char *out, unsigned out_size)
{
    char tmp[512];
    copy_bounded(tmp, sizeof tmp, path);

    int slash = -1;
    for (int i = (int) strlen(tmp) - 1; i >= 0; i--)
    {
        if (tmp[i] == '/') { slash = i; break; }
    }
    if (slash > 0)       tmp[slash] = '\0';
    else if (slash == 0) tmp[1]     = '\0';          // keep the leading "/"

    if (strlen(tmp) < strlen(root))                  // floor at the root
        copy_bounded(tmp, sizeof tmp, root);

    copy_bounded(out, out_size, tmp);
}
