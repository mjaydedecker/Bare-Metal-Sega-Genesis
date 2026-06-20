//
// src/settings/settings_store.cpp
//
// Bare Metal Sega Genesis
// See settings_store.h.
//

#include "settings_store.h"
#include <string.h>

static const char SETTINGS_PATH[] = "SD:/settings.txt";

SettingsStore::SettingsStore(Storage *pStorage)
:   m_pStorage(pStorage)
{
}

bool SettingsStore::Load(Settings *pOut)
{
    if (pOut == 0) return false;
    *pOut = Settings();               // defaults

    u8    *buf  = 0;
    size_t size = 0;
    if (!m_pStorage->Exists(SETTINGS_PATH) ||
        !m_pStorage->ReadFile(SETTINGS_PATH, &buf, &size))
    {
        Save(*pOut);                  // write a default file to edit
        return false;
    }

    // NUL-terminate a bounded copy for the parser.
    char   text[1024];
    size_t n = size < sizeof(text) - 1 ? size : sizeof(text) - 1;
    memcpy(text, buf, n);
    text[n] = '\0';
    delete[] buf;                     // Storage::ReadFile hands ownership over

    *pOut = parse_settings(text);
    return true;
}

bool SettingsStore::Save(const Settings &s)
{
    char text[256];
    serialize_settings(s, text, sizeof text);
    return m_pStorage->WriteFile(SETTINGS_PATH,
                                 (const u8 *) text, strlen(text));
}
