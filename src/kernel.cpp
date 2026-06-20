//
// kernel.cpp
//
// Bare Metal Sega Genesis
//

#include "kernel.h"
#include "input/joypad_map.h"   // GP_START, GP_SELECT bits for the menu hotkey

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
	m_SaveState (&m_Storage),
	m_Sram (&m_Storage),
	m_Canvas (&m_Display),
	m_RomMenu (&m_Canvas, &m_Gamepad, &m_Storage, &m_USBHCI),
	m_PauseMenu (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_SaveState),
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

	if (!m_Storage.Mount ())
	{
		m_Logger.Write (FromKernel, LogPanic, "SD card mount failed");
		return ShutdownHalt;
	}

	// libretro callbacks + core init: once for the whole session.
	retro_set_environment (environment_cb);
	retro_set_video_refresh (video_refresh_cb);
	retro_set_audio_sample (audio_sample_cb);
	retro_set_audio_sample_batch (audio_batch_cb);
	retro_set_input_poll (input_poll_cb);
	retro_set_input_state (input_state_cb);
	retro_init ();

	g_display = &m_Display;
	g_gamepad = &m_Gamepad;

	#define RETRO_DEVICE_MDPAD_6B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
	const unsigned HOTKEY = GP_START | GP_SELECT;

	boolean audioInited = FALSE;

	for (;;)   // browse <-> play
	{
		// --- Browse ---
		char romPath[300];
		if (!m_RomMenu.Run (romPath, sizeof romPath))
		{
			m_Logger.Write (FromKernel, LogPanic, "No ROMs found in /roms");
			return ShutdownHalt;   // RomMenu already drew the message
		}

		if (!m_Storage.ReadFile (romPath, &m_pROMBuffer, &m_nROMSize))
		{
			m_Canvas.Clear (0x0000);
			m_Canvas.DrawText (40, 40, "Failed to read ROM.", 0xF800, 0x0000);
			m_Canvas.DrawText (40, 40 + (int) m_Canvas.CharH (),
				"Returning to browser...", 0xFFFF, 0x0000);
			CTimer::SimpleMsDelay (2000);
			continue;
		}

		g_rom_data = m_pROMBuffer;
		g_rom_size = m_nROMSize;

		struct retro_game_info gameInfo;
		gameInfo.path = romPath;
		gameInfo.data = m_pROMBuffer;
		gameInfo.size = m_nROMSize;
		gameInfo.meta = "";

		if (!retro_load_game (&gameInfo))
		{
			delete[] m_pROMBuffer;
			m_pROMBuffer = 0;
			m_Canvas.Clear (0x0000);
			m_Canvas.DrawText (40, 40, "Failed to load ROM.", 0xF800, 0x0000);
			m_Canvas.DrawText (40, 40 + (int) m_Canvas.CharH (),
				"Returning to browser...", 0xFFFF, 0x0000);
			CTimer::SimpleMsDelay (2000);
			continue;
		}

		// Pacing parameters from the core's A/V info.
		struct retro_system_av_info avInfo;
		retro_get_system_av_info (&avInfo);
		unsigned sampleRate     = (unsigned) avInfo.timing.sample_rate;
		double   fps            = (double) avInfo.timing.fps;
		if (fps < 1.0 || fps > 61.0) fps = 60.0;
		unsigned framesPerVideo = sampleRate ? (unsigned) (sampleRate / fps) : 0;
		unsigned target         = framesPerVideo * 2;
		u64      period_us      = (u64) (1000000.0 / fps);

		// Audio: initialise once (Genesis sample rate is constant).
		if (!audioInited && sampleRate > 0 && m_Audio.Initialize (sampleRate))
		{
			audioInited = TRUE;
			g_audio = &m_Audio;
		}
		boolean audioOK = audioInited;

		retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
		retro_set_controller_port_device (1, RETRO_DEVICE_NONE);

		m_SaveState.SetGame (romPath);   // save/load target for this game
		m_Sram.SetGame (romPath);
		m_Sram.Load ();                  // restore battery SRAM if present

		// --- Play ---
		u64      next      = CTimer::GetClockTicks64 ();
		unsigned frame     = 0;
		boolean  ledOn     = FALSE;
		unsigned prevBtns  = 0;
		boolean  toBrowser = FALSE;

		for (;;)
		{
			m_USBHCI.UpdatePlugAndPlay ();
			m_Gamepad.Poll ();
			unsigned now     = m_Gamepad.Buttons ();
			unsigned pressed = now & ~prevBtns;
			prevBtns = now;

			// Hotkey: Start+Select both held, completed this frame.
			if ((pressed & HOTKEY) && (now & HOTKEY) == HOTKEY)
			{
				MenuAction action = m_PauseMenu.Run ();
				if (action == MenuAction::Reset)
				{
					retro_reset ();
				}
				else if (action == MenuAction::ReturnToBrowser)
				{
					toBrowser = TRUE;
				}
				prevBtns = m_Gamepad.Buttons ();          // resync after the menu
				next     = CTimer::GetClockTicks64 ();    // re-baseline pacing
				if (toBrowser) break;
				m_Display.ForceRepaint ();   // wipe the overlay from the letterbox bars
				continue;
			}

			retro_run ();
			m_Sram.Tick ();              // periodic dirty-checked SRAM auto-save

			next += period_us;
			u64 t = CTimer::GetClockTicks64 ();
			if (next < t) next = t;
			while (CTimer::GetClockTicks64 () < next) { }

			if (audioOK)
			{
				while (m_Audio.QueuedFrames () > target + framesPerVideo) { }
			}

			if (++frame >= 30)
			{
				frame = 0;
				ledOn = !ledOn;
				if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
			}
		}

		// --- Unload and return to the browser ---
		m_Sram.Save ();                  // flush battery SRAM before unloading
		retro_unload_game ();
		delete[] m_pROMBuffer;
		m_pROMBuffer = 0;
	}

	return ShutdownHalt;
}
