//
// kernel.cpp
//
// Bare Metal Sega Genesis
//

#include "kernel.h"

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
:	m_CPUThrottle (CPUSpeedMaximum),
	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE),   // TRUE: USB plug-and-play (gamepad)
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_Audio (&m_Interrupt),
	m_Gamepad (&m_DeviceNameService),
	m_Storage (),
	m_RomMenu (&m_Screen, &m_Gamepad, &m_Storage, &m_USBHCI),
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
		// USB gamepads enumerate asynchronously via plug-and-play; the pad is
		// acquired later in the frame loop (UpdatePlugAndPlay + Gamepad::Poll).
		m_Logger.Write (FromKernel, LogNotice, "Input: USB gamepad (plug-and-play)");
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice,
		"Bare Metal Sega Genesis — build " __DATE__ " " __TIME__);

	// Mount the SD card filesystem.
	if (!m_Storage.Mount ())
	{
		m_Logger.Write (FromKernel, LogPanic, "SD card mount failed");
		return ShutdownHalt;
	}

	// Browse SD:/roms and let the user pick a ROM (renders on the console).
	char romPath[300];
	if (!m_RomMenu.Run (romPath, sizeof romPath))
	{
		m_Logger.Write (FromKernel, LogPanic, "No ROMs found in /roms");
		return ShutdownHalt;
	}

	// Read the selected ROM.
	m_Logger.Write (FromKernel, LogNotice, "Loading ROM: %s", romPath);
	if (!m_Storage.ReadFile (romPath, &m_pROMBuffer, &m_nROMSize))
	{
		m_Logger.Write (FromKernel, LogPanic, "Failed to read ROM: %s", romPath);
		return ShutdownHalt;
	}

	// Hand the screen over from the menu console to the game framebuffer.
	if (!m_Display.Initialize ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Display init failed");
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
	gameInfo.path = romPath;
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

	m_Logger.Write (FromKernel, LogNotice, "M6: entering frame loop");

	// Pacing parameters from the core's A/V info.
	unsigned sampleRate     = (unsigned) avInfo.timing.sample_rate;
	double   fps            = (double) avInfo.timing.fps;
	if (fps < 1.0 || fps > 61.0) fps = 60.0;   // clamp to a sane range
	unsigned framesPerVideo = sampleRate ? (unsigned) (sampleRate / fps) : 0;
	unsigned target         = framesPerVideo * 2;   // ~2 video frames of latency

	// Initialise HDMI audio. On failure, fall back to video-only timer pacing.
	boolean audioOK = (sampleRate > 0) && m_Audio.Initialize (sampleRate);
	if (audioOK)
	{
		g_audio = &m_Audio;
		m_Logger.Write (FromKernel, LogNotice,
			"Audio: HDMI %u Hz, target %u frames", sampleRate, target);
	}
	else
	{
		g_audio = 0;
		m_Logger.Write (FromKernel, LogWarning,
			"Audio disabled; using timer pacing");
	}

	// Configure port 0 as a 6-button Genesis pad (port 1 unused), and point
	// the input callbacks at our gamepad.
	#define RETRO_DEVICE_MDPAD_6B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
	retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
	retro_set_controller_port_device (1, RETRO_DEVICE_NONE);
	g_gamepad = &m_Gamepad;

	// Point the video callback at our Display.
	g_display = &m_Display;

	u64      period_us = (u64) (1000000.0 / fps);   // per-frame period
	u64      next      = CTimer::GetClockTicks64 ();
	unsigned frame     = 0;
	boolean  ledOn     = FALSE;
	for (;;)
	{
		// Pump USB plug-and-play so a connected gamepad enumerates and
		// Gamepad::Poll() can acquire it (it appears shortly after boot).
		m_USBHCI.UpdatePlugAndPlay ();

		retro_run ();                       // -> video_refresh_cb / audio_*_cb

		// Pace to the core's frame rate via the timer. The core runs far
		// faster than realtime (~7-8 ms/frame), so this gives smooth full
		// speed. (Pacing on the audio queue instead throttled the core.)
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

		// Anti-drift / overflow guard: the timer and the audio clock differ
		// slightly, so the queue slowly creeps. If it climbs past a high
		// watermark, hold until it drains -- this locks the long-term rate to
		// the audio clock without throttling the normal cadence. No-op if
		// audio init failed (video-only).
		if (audioOK)
		{
			while (m_Audio.QueuedFrames () > target + framesPerVideo)
			{
				// drain toward the setpoint, then resume
			}
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
