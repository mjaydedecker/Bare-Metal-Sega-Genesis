//
// src/input/controller_store.cpp
//
// Bare Metal Sega Genesis
// See controller_store.h.
//

#include "controller_store.h"

static const char *PATH = "SD:/controllers.txt";

ControllerStore::ControllerStore(void)
:   m_count(0)
{
}

bool ControllerStore::Load(Storage *pStorage)
{
    m_count = 0;
    if (pStorage == 0 || !pStorage->Exists(PATH)) return false;

    u8    *buf  = 0;
    size_t size = 0;
    if (!pStorage->ReadFile(PATH, &buf, &size)) return false;

    // Walk lines in place (the buffer is our own copy).
    char *text = (char *) buf;
    size_t i = 0;
    while (i < size && m_count < MAX_CONTROLLERS)
    {
        size_t start = i;
        while (i < size && text[i] != '\n' && text[i] != '\r') i++;
        bool atEnd = (i >= size);
        if (!atEnd) text[i] = '\0';              // terminate this line
        ControllerCal c;
        if (controller_parse_line(text + start, &c)) m_cal[m_count++] = c;
        while (i < size && (text[i] == '\n' || text[i] == '\r' || text[i] == '\0')) i++;
    }

    delete[] buf;
    return true;
}

bool ControllerStore::Save(Storage *pStorage)
{
    if (pStorage == 0) return false;

    char out[64 + MAX_CONTROLLERS * 48];
    size_t len = 0;
    const char *hdr = "# vid:pid=A,B,X,Y,L,R,Start,Select (raw HID bits, 255=unmapped)\n";
    for (const char *p = hdr; *p && len + 1 < sizeof out; p++) out[len++] = *p;

    for (unsigned k = 0; k < m_count; k++)
    {
        char line[48];
        controller_serialize_line(m_cal[k], line, sizeof line);
        for (const char *p = line; *p && len + 1 < sizeof out; p++) out[len++] = *p;
        if (len + 1 < sizeof out) out[len++] = '\n';
    }

    return pStorage->WriteFile(PATH, (const u8 *) out, len);
}

void ControllerStore::Upsert(const ControllerCal &c)
{
    for (unsigned k = 0; k < m_count; k++)
        if (m_cal[k].vid == c.vid && m_cal[k].pid == c.pid) { m_cal[k] = c; return; }
    if (m_count < MAX_CONTROLLERS) m_cal[m_count++] = c;
}
