//
// storage/sdcard.h
//
// Bare Metal Sega Genesis
// Thin wrapper around Circle's CFATFileSystem and CEMMCDevice.
//
// Notes on Circle's FatFS limitations:
//   - Root directory only: file titles are plain names with no path separator.
//   - Filenames are 8.3 format (maximum 12 characters including the dot).
//   - All ROM and SRAM files must reside in the root of the SD card.
//

#ifndef _storage_sdcard_h
#define _storage_sdcard_h

#include <circle/devicenameservice.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/types.h>

class SDCard
{
public:
	SDCard (CFATFileSystem &rFileSystem, CDeviceNameService &rDeviceNameService);

	// Mount the FAT32 partition on the onboard EMMC/SD card.
	// Must be called once before any ReadFile / WriteFile calls.
	boolean Mount (void);

	// Read an entire file from the SD card root into a newly allocated buffer.
	// On success: *ppBuffer is set to a new[] allocation; caller must delete[] it.
	//             *pSize is set to the file size in bytes.
	// On failure: logs the reason and returns FALSE. *ppBuffer and *pSize are unchanged.
	boolean ReadFile (const char *pTitle, u8 **ppBuffer, size_t *pSize);

	// Write pData to a file in the SD card root, creating or overwriting it.
	// Used for SRAM persistence (M9).
	boolean WriteFile (const char *pTitle, const u8 *pData, size_t nSize);

private:
	// Scan the root directory to get the size of a file before allocating a buffer.
	boolean GetFileSize (const char *pTitle, unsigned *pSize);

	CFATFileSystem     &m_FileSystem;
	CDeviceNameService &m_DeviceNameService;
	boolean             m_bMounted;
};

#endif
