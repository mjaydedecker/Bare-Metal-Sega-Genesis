//
// src/menu/rom_layout.h
//
// Bare Metal Sega Genesis
// Pure layout math for the ROM browser (scrollbar geometry, extension split).
// No Circle deps (host-testable).
//
#ifndef _menu_rom_layout_h
#define _menu_rom_layout_h

struct ScrollThumb { int y; int h; };

// Thumb geometry within a vertical track of height track_h px, for a list of
// `count` items showing `visible` at once, scrolled so `top` is first visible.
ScrollThumb scrollbar_thumb(int track_h, int count, int visible, int top);

// Offset of the extension dot for display dimming: index of the last '.' if it
// has >=1 char before and >=1 char after, else -1.
int rom_ext_offset(const char *name);

#endif
