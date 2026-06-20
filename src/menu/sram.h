//
// src/menu/sram.h
//
// Bare Metal Sega Genesis
// Persist battery-backed cartridge SRAM to SD: load at ROM start, auto-save
// periodically (when changed) and on ROM change. No-op for games without SRAM.
//

#ifndef _menu_sram_h
#define _menu_sram_h

#include <circle/types.h>
#include "../storage/storage.h"

class Sram
{
public:
    Sram(Storage *pStorage);
    ~Sram();

    void SetGame(const char *romPath);  // (re)alloc dirty-check snapshot for this game
    void Load(void);                    // .srm -> core SRAM (if present and file exists)
    void Save(void);                    // core SRAM -> .srm (if present)
    void Tick(void);                    // ~10s dirty-checked auto-save

private:
    bool Present(u8 **ppData, size_t *pSize);   // game has SRAM?

    Storage *m_pStorage;
    char     m_romPath[300];
    u8      *m_pSnapshot;   // last-written copy, for the dirty check
    size_t   m_SnapSize;
    u64      m_LastCheck;   // CTimer ticks (us) of the last Tick action
};

#endif
