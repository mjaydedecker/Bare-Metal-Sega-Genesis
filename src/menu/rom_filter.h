//
// src/menu/rom_filter.h
//
// Bare Metal Sega Genesis
// Pure predicate: is this filename a Genesis ROM we should list?
//

#ifndef _menu_rom_filter_h
#define _menu_rom_filter_h

// True if name ends (case-insensitively) in ".md", ".bin", or ".gen", with at
// least one character before the extension. A null/empty name is false.
bool is_rom_filename(const char *name);

#endif
