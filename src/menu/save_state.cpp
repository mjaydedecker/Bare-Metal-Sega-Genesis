//
// src/menu/save_state.cpp
//
// Bare Metal Sega Genesis
// See save_state.h.
//

#include "save_state.h"
#include "save_path.h"
#include <libretro.h>

static void copy_str(char *dst, const char *src, unsigned n)
{
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < n; i++) dst[i] = src[i];
    dst[i] = '\0';
}

SaveState::SaveState(Storage *pStorage)
:   m_pStorage(pStorage)
{
    m_romPath[0] = '\0';
}

void SaveState::SetGame(const char *romPath)
{
    copy_str(m_romPath, romPath, sizeof m_romPath);
}

bool SaveState::Occupied(int slot)
{
    char path[320];
    state_path(m_romPath, slot, path, sizeof path);
    return m_pStorage->Exists(path);
}

bool SaveState::Save(int slot)
{
    m_pStorage->MakeDir("SD:/saves");

    size_t size = retro_serialize_size();
    if (size == 0) return false;

    u8 *buf = new u8[size];
    if (!retro_serialize(buf, size))
    {
        delete[] buf;
        return false;
    }

    char path[320];
    state_path(m_romPath, slot, path, sizeof path);
    bool ok = m_pStorage->WriteFile(path, buf, size);
    delete[] buf;
    return ok;
}

bool SaveState::Load(int slot)
{
    char path[320];
    state_path(m_romPath, slot, path, sizeof path);

    u8    *buf  = 0;
    size_t size = 0;
    if (!m_pStorage->ReadFile(path, &buf, &size))
    {
        return false;
    }

    bool ok = retro_unserialize(buf, size);
    delete[] buf;
    return ok;
}
