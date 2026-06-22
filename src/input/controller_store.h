//
// src/input/controller_store.h
//
// Bare Metal Sega Genesis
// Loads/saves per-controller calibrations from SD:/controllers.txt and holds them
// for the Gamepad decoder. Parsing/serialization is the pure controller_map.
//

#ifndef _input_controller_store_h
#define _input_controller_store_h

#include "controller_map.h"
#include "../storage/storage.h"

class ControllerStore
{
public:
    static const unsigned MAX_CONTROLLERS = 16;

    ControllerStore(void);

    // Read SD:/controllers.txt (missing/empty -> 0 entries). Returns true if the
    // file was read.
    bool Load(Storage *pStorage);
    // Write all entries back to SD:/controllers.txt.
    bool Save(Storage *pStorage);

    const ControllerCal *Data(void) const { return m_cal; }
    unsigned             Count(void) const { return m_count; }

    // Replace the entry with the same vid/pid, or append (bounded by
    // MAX_CONTROLLERS — a new vid/pid past the limit is dropped).
    void Upsert(const ControllerCal &c);

private:
    ControllerCal m_cal[MAX_CONTROLLERS];
    unsigned      m_count;
};

#endif
