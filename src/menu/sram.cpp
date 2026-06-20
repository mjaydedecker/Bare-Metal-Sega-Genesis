//
// src/menu/sram.cpp
//
// Bare Metal Sega Genesis
// See sram.h.
//

#include "sram.h"
#include "save_path.h"
#include <libretro.h>
#include <circle/timer.h>
#include <circle/util.h>     // memcpy, memcmp
#include <circle/logger.h>

static const char FromSram[] = "sram";
static const u64  INTERVAL_US = 10ULL * 1000000ULL;   // 10 s (clock is 1 MHz)

static void copy_str(char *dst, const char *src, unsigned n)
{
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < n; i++) dst[i] = src[i];
    dst[i] = '\0';
}

Sram::Sram(Storage *pStorage)
:   m_pStorage(pStorage), m_pSnapshot(0), m_SnapSize(0), m_LastCheck(0)
{
    m_romPath[0] = '\0';
}

Sram::~Sram()
{
    delete[] m_pSnapshot;
    m_pSnapshot = 0;
}

bool Sram::Present(u8 **ppData, size_t *pSize)
{
    size_t size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    u8    *data = (u8 *) retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (size == 0 || data == 0) return false;
    *ppData = data;
    *pSize  = size;
    return true;
}

void Sram::SetGame(const char *romPath)
{
    copy_str(m_romPath, romPath, sizeof m_romPath);

    delete[] m_pSnapshot;
    m_pSnapshot = 0;
    m_SnapSize  = 0;

    u8    *data;
    size_t size;
    if (Present(&data, &size))
    {
        m_pSnapshot = new u8[size];
        memcpy(m_pSnapshot, data, size);
        m_SnapSize = size;
    }
    m_LastCheck = CTimer::GetClockTicks64();
}

void Sram::Load(void)
{
    u8    *data;
    size_t size;
    if (!Present(&data, &size)) return;

    char path[320];
    sram_path(m_romPath, path, sizeof path);

    u8    *buf   = 0;
    size_t fsize = 0;
    if (!m_pStorage->ReadFile(path, &buf, &fsize)) return;   // absent or error

    if (fsize == size)
    {
        memcpy(data, buf, size);
        if (m_pSnapshot != 0 && m_SnapSize == size) memcpy(m_pSnapshot, buf, size);
        CLogger::Get()->Write(FromSram, LogNotice, "SRAM loaded (%u bytes)",
                              (unsigned) size);
    }
    else
    {
        CLogger::Get()->Write(FromSram, LogWarning,
            "SRAM size mismatch (file %u, core %u); ignored",
            (unsigned) fsize, (unsigned) size);
    }
    delete[] buf;
}

void Sram::Save(void)
{
    u8    *data;
    size_t size;
    if (!Present(&data, &size)) return;

    m_pStorage->MakeDir("SD:/saves");
    char path[320];
    sram_path(m_romPath, path, sizeof path);
    if (m_pStorage->WriteFile(path, data, size))
    {
        if (m_pSnapshot != 0 && m_SnapSize == size) memcpy(m_pSnapshot, data, size);
    }
}

void Sram::Tick(void)
{
    u8    *data;
    size_t size;
    if (!Present(&data, &size)) return;

    u64 now = CTimer::GetClockTicks64();
    if (now - m_LastCheck < INTERVAL_US) return;
    m_LastCheck = now;

    if (m_pSnapshot == 0 || m_SnapSize != size) return;       // safety
    if (memcmp(data, m_pSnapshot, size) != 0)
    {
        m_pStorage->MakeDir("SD:/saves");
        char path[320];
        sram_path(m_romPath, path, sizeof path);
        if (m_pStorage->WriteFile(path, data, size))
        {
            memcpy(m_pSnapshot, data, size);
        }
    }
}
