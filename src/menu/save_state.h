//
// src/menu/save_state.h
//
// Bare Metal Sega Genesis
// Save/load the libretro core state to SD-card slots for the current game.
//

#ifndef _menu_save_state_h
#define _menu_save_state_h

#include <circle/types.h>
#include "../storage/storage.h"

class SaveState
{
public:
    SaveState(Storage *pStorage);

    void SetGame(const char *romPath);   // copies the current ROM path

    bool Occupied(int slot);             // a save file exists for this slot (1..4)
    bool Save(int slot);                 // serialize the core -> slot file
    bool Load(int slot);                 // slot file -> unserialize the core

private:
    Storage *m_pStorage;
    char     m_romPath[300];
};

#endif
