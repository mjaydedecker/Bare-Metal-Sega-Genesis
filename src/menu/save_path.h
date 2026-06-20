//
// src/menu/save_path.h
//
// Bare Metal Sega Genesis
// Pure: build the save-state file path for a ROM + slot.
//

#ifndef _menu_save_path_h
#define _menu_save_path_h

// out = "SD:/saves/<filename>.state<slot>", where <filename> is romPath with its
// directory stripped (extension kept) and slot is 1..4. Bounded by out_size.
void state_path(const char *romPath, int slot, char *out, unsigned out_size);

// out = "SD:/saves/<filename>.srm", where <filename> is romPath with its
// directory stripped (extension kept). Bounded by out_size.
void sram_path(const char *romPath, char *out, unsigned out_size);

#endif
