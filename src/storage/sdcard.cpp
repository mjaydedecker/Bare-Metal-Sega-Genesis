//
// storage/sdcard.cpp
//
// Bare Metal Sega Genesis
//

#include "sdcard.h"
#include <circle/logger.h>
#include <circle/string.h>

static const char FromSDCard[] = "sdcard";

// The EMMC partition device registered by CEMMCDevice::Initialize().
#define EMMC_PARTITION "emmc1-1"

// Case-insensitive filename comparison.
// Circle's FatFS stores titles in uppercase; callers may pass mixed case.
static boolean TitleMatch (const char *pA, const char *pB)
{
	while (*pA && *pB)
	{
		char a = (*pA >= 'a' && *pA <= 'z') ? (char)(*pA - 32) : *pA;
		char b = (*pB >= 'a' && *pB <= 'z') ? (char)(*pB - 32) : *pB;
		if (a != b) return FALSE;
		pA++;
		pB++;
	}
	return *pA == '\0' && *pB == '\0';
}

SDCard::SDCard (CFATFileSystem &rFileSystem, CDeviceNameService &rDeviceNameService)
:	m_FileSystem (rFileSystem),
	m_DeviceNameService (rDeviceNameService),
	m_bMounted (FALSE)
{
}

boolean SDCard::Mount (void)
{
	CDevice *pPartition = m_DeviceNameService.GetDevice (EMMC_PARTITION, TRUE);
	if (pPartition == 0)
	{
		CLogger::Get ()->Write (FromSDCard, LogError,
			"Partition not found: %s", EMMC_PARTITION);
		return FALSE;
	}

	if (!m_FileSystem.Mount (pPartition))
	{
		CLogger::Get ()->Write (FromSDCard, LogError,
			"Cannot mount FAT partition: %s", EMMC_PARTITION);
		return FALSE;
	}

	m_bMounted = TRUE;
	CLogger::Get ()->Write (FromSDCard, LogNotice, "SD card mounted");
	return TRUE;
}

boolean SDCard::GetFileSize (const char *pTitle, unsigned *pSize)
{
	TDirentry         Entry;
	TFindCurrentEntry Current;

	unsigned nEntry = m_FileSystem.RootFindFirst (&Entry, &Current);
	while (nEntry != 0)
	{
		if (TitleMatch (Entry.chTitle, pTitle))
		{
			*pSize = Entry.nSize;
			return TRUE;
		}
		nEntry = m_FileSystem.RootFindNext (&Entry, &Current);
	}
	return FALSE;
}

boolean SDCard::ReadFile (const char *pTitle, u8 **ppBuffer, size_t *pSize)
{
	unsigned nFileSize;
	if (!GetFileSize (pTitle, &nFileSize))
	{
		CLogger::Get ()->Write (FromSDCard, LogError,
			"File not found: %s", pTitle);
		return FALSE;
	}

	unsigned hFile = m_FileSystem.FileOpen (pTitle);
	if (hFile == 0)
	{
		CLogger::Get ()->Write (FromSDCard, LogError,
			"Cannot open file: %s", pTitle);
		return FALSE;
	}

	u8 *pBuffer = new u8[nFileSize];

	unsigned nRead = m_FileSystem.FileRead (hFile, pBuffer, nFileSize);
	m_FileSystem.FileClose (hFile);

	if (nRead != nFileSize)
	{
		CLogger::Get ()->Write (FromSDCard, LogError,
			"Read error on %s: expected %u bytes, got %u",
			pTitle, nFileSize, nRead);
		delete[] pBuffer;
		return FALSE;
	}

	*ppBuffer = pBuffer;
	*pSize    = nFileSize;
	return TRUE;
}

boolean SDCard::WriteFile (const char *pTitle, const u8 *pData, size_t nSize)
{
	unsigned hFile = m_FileSystem.FileCreate (pTitle);
	if (hFile == 0)
	{
		CLogger::Get ()->Write (FromSDCard, LogError,
			"Cannot create file: %s", pTitle);
		return FALSE;
	}

	unsigned nWritten = m_FileSystem.FileWrite (hFile, pData, nSize);
	m_FileSystem.FileClose (hFile);

	if (nWritten != nSize)
	{
		CLogger::Get ()->Write (FromSDCard, LogError,
			"Write error on %s: wrote %u of %u bytes",
			pTitle, nWritten, (unsigned) nSize);
		return FALSE;
	}

	return TRUE;
}
