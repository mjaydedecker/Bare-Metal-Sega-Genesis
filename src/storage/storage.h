//
// src/storage/storage.h
//
// Bare Metal Sega Genesis
// Storage over Circle's add-on ChaN FatFS (long filenames + subdirectories).
// All paths are volume-qualified, e.g. "SD:/roms". Replaces sdcard.{h,cpp}.
//

#ifndef _storage_storage_h
#define _storage_storage_h

#include <circle/types.h>
#include "ff.h"   // ChaN FatFS (include path: libs/circle/addon/fatfs)

struct Entry
{
    char name[256];   // filename only; full path = dir + "/" + name
    bool is_dir;
};

class Storage
{
public:
    Storage(void);

    // Mount the SD card volume ("SD:" -> the emmc1 device). Call once at boot.
    bool Mount(void);

    // List `dir`: subdirectories first (is_dir=true), then ROM files
    // (is_dir=false). Returns the entry count, or -1 if `dir` can't be opened.
    int ListDir(const char *dir, Entry *out, int max);

    // Read an entire file into a new[] buffer. Caller owns *ppBuffer.
    bool ReadFile(const char *path, u8 **ppBuffer, size_t *pSize);

private:
    FATFS m_FS;
    bool  m_bMounted;
};

#endif
