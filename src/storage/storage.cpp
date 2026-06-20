//
// src/storage/storage.cpp
//
// Bare Metal Sega Genesis
// See storage.h.
//

#include "storage.h"
#include "../menu/rom_filter.h"
#include <circle/logger.h>

static const char FromStorage[] = "storage";

static void copy_name(char *dst, const char *src, unsigned dstsize)
{
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < dstsize; i++) dst[i] = src[i];
    dst[i] = '\0';
}

// Case-insensitive name comparison (a<b: <0, equal: 0, a>b: >0).
static int ci_cmp(const char *a, const char *b)
{
    for (;;)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char) (ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char) (cb + 32);
        if (ca != cb) return (int) (unsigned char) ca - (int) (unsigned char) cb;
        if (ca == '\0') return 0;
        a++; b++;
    }
}

// Insertion sort `n` entries by name, case-insensitive. n is small (one dir).
static void sort_entries(Entry *e, int n)
{
    for (int i = 1; i < n; i++)
    {
        Entry key = e[i];
        int j = i - 1;
        while (j >= 0 && ci_cmp(e[j].name, key.name) > 0)
        {
            e[j + 1] = e[j];
            j--;
        }
        e[j + 1] = key;
    }
}

Storage::Storage(void)
:   m_bMounted(false)
{
}

bool Storage::Mount(void)
{
    FRESULT r = f_mount(&m_FS, "SD:", 1);   // 1 = mount immediately
    if (r != FR_OK)
    {
        CLogger::Get()->Write(FromStorage, LogError, "f_mount failed (%d)", (int) r);
        return false;
    }
    m_bMounted = true;
    CLogger::Get()->Write(FromStorage, LogNotice, "SD card mounted (FatFS)");
    return true;
}

int Storage::ListDir(const char *dir, Entry *out, int max)
{
    DIR d;
    if (f_opendir(&d, dir) != FR_OK)
        return -1;

    int n = 0;
    FILINFO fno;

    // Pass 1: subdirectories.
    while (n < max && f_readdir(&d, &fno) == FR_OK && fno.fname[0] != '\0')
    {
        if (fno.fattrib & AM_DIR)
        {
            copy_name(out[n].name, fno.fname, sizeof out[n].name);
            out[n].is_dir = true;
            n++;
        }
    }
    int nDirs = n;

    // Pass 2: ROM files. f_readdir(&d, 0) rewinds the directory.
    f_readdir(&d, 0);
    while (n < max && f_readdir(&d, &fno) == FR_OK && fno.fname[0] != '\0')
    {
        if (!(fno.fattrib & AM_DIR) && is_rom_filename(fno.fname))
        {
            copy_name(out[n].name, fno.fname, sizeof out[n].name);
            out[n].is_dir = false;
            n++;
        }
    }

    f_closedir(&d);

    // Sort each group alphabetically (case-insensitive): directories, then files.
    sort_entries(out, nDirs);
    sort_entries(out + nDirs, n - nDirs);
    return n;
}

bool Storage::ReadFile(const char *path, u8 **ppBuffer, size_t *pSize)
{
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        CLogger::Get()->Write(FromStorage, LogError, "Cannot open: %s", path);
        return false;
    }

    unsigned size = (unsigned) f_size(&file);
    u8 *buf = new u8[size];

    UINT read = 0;
    FRESULT r = f_read(&file, buf, size, &read);
    f_close(&file);

    if (r != FR_OK || read != size)
    {
        CLogger::Get()->Write(FromStorage, LogError, "Read error: %s", path);
        delete[] buf;
        return false;
    }

    *ppBuffer = buf;
    *pSize    = size;
    return true;
}
