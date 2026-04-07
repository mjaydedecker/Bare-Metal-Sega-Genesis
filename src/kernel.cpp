//
// kernel.cpp
//
// Bare Metal Sega Genesis
//

#include "kernel.h"

// ROM file to load from the SD card root.
// Circle's FatFS supports root directory only with 8.3 filenames.
// Copy your Genesis ROM to the SD card and rename it to this name.
#define ROM_TITLE "GAME.MD"

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_Sound (&m_Interrupt),
	m_SDCard (m_FileSystem, m_DeviceNameService),
	m_pROMBuffer (0),
	m_nROMSize (0)
{
	m_ActLED.Blink (5);
}

CKernel::~CKernel (void)
{
	delete[] m_pROMBuffer;
	m_pROMBuffer = 0;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising USB");
		bOK = m_USBHCI.Initialize ();
	}

	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising SD card");
		bOK = m_EMMC.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice,
		"Bare Metal Sega Genesis — build " __DATE__ " " __TIME__);

	// Mount the SD card filesystem.
	if (!m_SDCard.Mount ())
	{
		m_Logger.Write (FromKernel, LogPanic, "SD card mount failed");
		return ShutdownHalt;
	}

	// Load ROM from the SD card root.
	// Rename your ROM to ROM_TITLE on the SD card before booting.
	m_Logger.Write (FromKernel, LogNotice, "Loading ROM: %s", ROM_TITLE);
	if (!m_SDCard.ReadFile (ROM_TITLE, &m_pROMBuffer, &m_nROMSize))
	{
		m_Logger.Write (FromKernel, LogPanic,
			"ROM not found — copy your Genesis ROM to the SD card root as %s",
			ROM_TITLE);
		return ShutdownHalt;
	}

	m_Logger.Write (FromKernel, LogNotice,
		"ROM loaded: %u bytes", (unsigned) m_nROMSize);

	// Spin until emulation core is wired in (M4).
	m_Logger.Write (FromKernel, LogNotice, "M3 complete — standing by.");
	for (unsigned i = 0; ; i++)
	{
		m_Timer.SimpleMsDelay (1000);
		if (i % 2 == 0) m_ActLED.On ();
		else             m_ActLED.Off ();
	}

	return ShutdownHalt;
}
