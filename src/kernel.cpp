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
			pTarget = &m_Serial;   // M5: framebuffer is dedicated to video
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

	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising video");
		bOK = m_Display.Initialize ();
		if (!bOK)
		{
			m_Logger.Write (FromKernel, LogPanic, "Display init failed");
		}
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

	// Wire up libretro callbacks before retro_init().
	m_Logger.Write (FromKernel, LogNotice, "Setting callbacks");
	retro_set_environment (environment_cb);
	retro_set_video_refresh (video_refresh_cb);
	retro_set_audio_sample (audio_sample_cb);
	retro_set_audio_sample_batch (audio_batch_cb);
	retro_set_input_poll (input_poll_cb);
	retro_set_input_state (input_state_cb);

	// Initialise the emulation core.
	m_Logger.Write (FromKernel, LogNotice, "Calling retro_init");
	retro_init ();
	m_Logger.Write (FromKernel, LogNotice, "retro_init done");

	// Expose ROM to the environment callback so GET_GAME_INFO_EXT works
	// and the core loads from memory rather than the (unsupported) filesystem.
	g_rom_data = m_pROMBuffer;
	g_rom_size = m_nROMSize;

	// Load the ROM.
	struct retro_game_info gameInfo;
	gameInfo.path = ROM_TITLE;
	gameInfo.data = m_pROMBuffer;
	gameInfo.size = m_nROMSize;
	gameInfo.meta = "";

	m_Logger.Write (FromKernel, LogNotice, "Calling retro_load_game");
	if (!retro_load_game (&gameInfo))
	{
		m_Logger.Write (FromKernel, LogPanic, "retro_load_game failed");
		retro_deinit ();
		return ShutdownHalt;
	}
	m_Logger.Write (FromKernel, LogNotice, "retro_load_game done");

	// Log A/V geometry so we can verify the core is alive.
	struct retro_system_av_info avInfo;
	retro_get_system_av_info (&avInfo);
	m_Logger.Write (FromKernel, LogNotice,
		"AV info: %u x %u @ %.2f fps",
		avInfo.geometry.base_width,
		avInfo.geometry.base_height,
		(double) avInfo.timing.fps);

	m_Logger.Write (FromKernel, LogNotice, "M5: entering frame loop");

	// Point the video callback at our Display.
	g_display = &m_Display;

	// Pace to the core's reported frame rate. Approximate for M5; M6 audio
	// will become the real sync source.
	double fps = (double) avInfo.timing.fps;
	if (fps < 1.0) fps = 60.0;
	u64 period_us = (u64) (1000000.0 / fps);

	u64 next = CTimer::GetClockTicks64 ();
	unsigned frame = 0;
	boolean ledOn = FALSE;
	for (;;)
	{
		retro_run ();                       // -> video_refresh_cb -> Blit

		next += period_us;
		u64 now = CTimer::GetClockTicks64 ();
		if (next < now)                     // running behind: drop the slack
		{
			next = now;
		}
		while (CTimer::GetClockTicks64 () < next)
		{
			// spin to the frame deadline
		}

		if (++frame >= 30)                  // ~0.5s liveness blink
		{
			frame = 0;
			ledOn = !ledOn;
			if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
		}
	}

	return ShutdownHalt;
}
