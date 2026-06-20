//
// src/settings/settings_store.h
//
// Bare Metal Sega Genesis
// Persists Settings to SD:/settings.txt via Storage. Separate from the Pi
// firmware's config.txt (which the GPU reads at boot — never written here).
//

#ifndef _settings_settings_store_h
#define _settings_settings_store_h

#include "settings.h"
#include "../storage/storage.h"

class SettingsStore
{
public:
    explicit SettingsStore(Storage *pStorage);

    // Read SD:/settings.txt into *pOut. If the file is missing or unreadable,
    // *pOut keeps default values and a default file is written. Returns true
    // if an existing file was read, false if defaults were used.
    bool Load(Settings *pOut);

    // Write the settings to SD:/settings.txt. Returns false on I/O failure.
    bool Save(const Settings &s);

private:
    Storage *m_pStorage;
};

#endif
