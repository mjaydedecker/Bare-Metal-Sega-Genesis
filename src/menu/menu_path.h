//
// src/menu/menu_path.h
//
// Bare Metal Sega Genesis
// Pure path helpers for the ROM browser. Alias-safe: `out` may be the same
// buffer as `base`/`path`.
//

#ifndef _menu_menu_path_h
#define _menu_menu_path_h

// out = base + "/" + name (bounded by out_size).
void path_join(const char *base, const char *name, char *out, unsigned out_size);

// out = path with its last "/component" removed, but never shorter than `root`
// (calling it at `root` yields `root`). Bounded by out_size.
void path_parent(const char *path, const char *root, char *out, unsigned out_size);

#endif
