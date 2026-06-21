//
// kernel.cpp
//
// Bare Metal Sega Genesis
//

#include "kernel.h"
#include "input/joypad_map.h"   // GP_START, GP_SELECT bits for the menu hotkey
#include "input/hotkey.h"       // decode_hotkey, InGameAction
#include "audio/audio_util.h"   // classify_queue, AQ_* for metrics

static const char FromKernel[] = "kernel";

// Short label for the active scale mode, for the HUD.
static const char *scale_name (ScaleMode m)
{
	return m == ScaleMode::Stretch ? "stretch"
	     : m == ScaleMode::Aspect  ? "aspect"
	                               : "integer";
}

// Format "Volume NNN" into out (>= 12 bytes) without snprintf.
static void vol_toast (char *out, unsigned v)
{
	const char *p = "Volume ";
	int i = 0;
	while (*p) out[i++] = *p++;
	char rev[4];
	int  n = 0;
	if (v == 0) rev[n++] = '0';
	else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
	while (n) out[i++] = rev[--n];
	out[i] = '\0';
}

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
	m_Overlay (&m_Canvas),
	m_RomMenu (&m_Canvas, &m_Gamepad, &m_Storage, &m_USBHCI),
	m_Settings (),
	m_SettingsStore (&m_Storage),
	m_VideoModeScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display),
	m_ControlsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore),
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display, &m_Audio, &m_ControlsScreen, &m_VideoModeScreen, &m_Overlay),
	m_PauseMenu (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_SaveState, &m_SettingsScreen),
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

	// Load user settings and apply them before the core reads variables.
	m_SettingsStore.Load (&m_Settings);
	m_Display.SetScaleMode (m_Settings.scale_mode);
	m_Display.SetVsync (m_Settings.vsync);
	g_widescreen = m_Settings.widescreen;
	m_Audio.SetVolume (m_Settings.volume);
	m_Audio.SetMute (m_Settings.mute);
	m_Overlay.SetEnabled (m_Settings.debug_overlay);
	g_region_value = region_core_value (m_Settings.region);
	g_map0 = &m_Settings.map1;
	g_map1 = &m_Settings.map2;
	g_hotkey_hold_mask = hotkey_hold_mask (m_Settings.hotkeys);
	if (m_Settings.video_mode != VideoMode::Native)
	{
		unsigned vmW, vmH;
		video_mode_dims (m_Settings.video_mode, vmW, vmH);
		if (!m_Display.SetMode (vmW, vmH))
		{
			m_Logger.Write (FromKernel, LogWarning,
				"Saved video mode unavailable; using native");
		}
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

	boolean audioInited = FALSE;
	boolean firstBoot   = TRUE;

	for (;;)   // browse <-> play
	{
		// --- Browse (or auto-launch on the very first pass) ---
		char romPath[300];
		boolean autoLaunched = FALSE;
		if (firstBoot && m_Settings.auto_launch_rom[0] != '\0' &&
		    m_Storage.Exists (m_Settings.auto_launch_rom))
		{
			unsigned i = 0;
			for (; m_Settings.auto_launch_rom[i] && i < sizeof romPath - 1; i++)
				romPath[i] = m_Settings.auto_launch_rom[i];
			romPath[i] = '\0';
			autoLaunched = TRUE;
		}
		firstBoot = FALSE;

		if (!autoLaunched && !m_RomMenu.Run (romPath, sizeof romPath))
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
		if (!audioInited && sampleRate > 0 && m_Audio.Initialize (sampleRate, m_Settings.audio_output))
		{
			audioInited = TRUE;
			g_audio = &m_Audio;
		}
		boolean audioOK = audioInited;

		retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
		retro_set_controller_port_device (1, RETRO_DEVICE_MDPAD_6B);

		m_SaveState.SetGame (romPath);   // save/load target for this game
		m_Sram.SetGame (romPath);
		m_Sram.Load ();                  // restore battery SRAM if present
		m_SettingsScreen.SetCurrentRom (romPath);   // auto-launch toggle target

		// --- Play ---
		u64      next      = CTimer::GetClockTicks64 ();
		unsigned fpsMeasured = (unsigned) fps;          // seed with nominal
		unsigned fpsFrames   = 0;
		u64      fpsWindow   = next;                    // 1 s window start (us)
		unsigned frame     = 0;
		unsigned logCtr    = 0;
		boolean  ledOn     = FALSE;
		unsigned prevBtns  = 0;
		unsigned prevP1    = 0;   // player-1 button state for in-game hotkeys
		boolean  toBrowser = FALSE;

		for (;;)
		{
			m_USBHCI.UpdatePlugAndPlay ();
			m_Gamepad.Poll ();
			unsigned now     = m_Gamepad.MenuButtons ();
			unsigned pressed = now & ~prevBtns;
			prevBtns = now;

			// Configurable menu hotkey (both buttons held), read live.
			unsigned hotkey = hotkey_mask (m_Settings.menu_hotkey);
			if ((pressed & hotkey) && (now & hotkey) == hotkey)
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
				prevBtns = m_Gamepad.MenuButtons ();       // resync after the menu
				next     = CTimer::GetClockTicks64 ();    // re-baseline pacing
				if (toBrowser) break;
				m_Display.ForceRepaint ();   // wipe the overlay from the letterbox bars
				continue;
			}

			// In-game action hotkeys (player 1: Select + button), read live.
			unsigned     p1now     = m_Gamepad.Buttons (0);
			unsigned     p1pressed = p1now & ~prevP1;
			prevP1 = p1now;
			InGameAction act = decode_hotkey (p1now, p1pressed, m_Settings.hotkeys);
			switch (act)
			{
			case InGameAction::QuickSave:
				m_Overlay.ShowToast (m_SaveState.Save (1) ? "Quick-saved"
				                                          : "Save failed");
				break;
			case InGameAction::QuickLoad:
				if (!m_SaveState.Occupied (1))
					m_Overlay.ShowToast ("No quick save");
				else
					m_Overlay.ShowToast (m_SaveState.Load (1) ? "Quick-loaded"
					                                          : "Load failed");
				break;
			case InGameAction::VolUp:
			case InGameAction::VolDown:
			{
				int dir = (act == InGameAction::VolUp) ? 10 : -10;
				int v   = (int) m_Settings.volume + dir;
				if (v < 0)   v = 0;
				if (v > 100) v = 100;
				m_Settings.volume = (unsigned) v;
				m_Audio.SetVolume (m_Settings.volume);
				m_SettingsStore.Save (m_Settings);
				char t[12];
				vol_toast (t, m_Settings.volume);
				m_Overlay.ShowToast (t);
				break;
			}
			case InGameAction::ToggleHud:
				m_Settings.debug_overlay = !m_Settings.debug_overlay;
				m_Overlay.SetEnabled (m_Settings.debug_overlay);
				m_SettingsStore.Save (m_Settings);
				m_Overlay.ShowToast (m_Settings.debug_overlay ? "HUD on" : "HUD off");
				if (!m_Settings.debug_overlay) m_Display.ForceRepaint ();
				break;
			case InGameAction::Mute:
				m_Settings.mute = !m_Settings.mute;
				m_Audio.SetMute (m_Settings.mute);
				m_SettingsStore.Save (m_Settings);
				m_Overlay.ShowToast (m_Settings.mute ? "Muted" : "Unmuted");
				break;
			case InGameAction::None:
				break;
			}

			retro_run ();
			m_Sram.Tick ();              // periodic dirty-checked SRAM auto-save

			// Measured FPS: count frames, recompute every ~1 s (ticks are us).
			fpsFrames++;
			u64 fpsNow = CTimer::GetClockTicks64 ();
			if (fpsNow - fpsWindow >= 1000000ULL)
			{
				fpsMeasured = fpsFrames;
				fpsFrames   = 0;
				fpsWindow   = fpsNow;
			}

			if (m_Overlay.Enabled ())
			{
				HudStats st;
				st.fps       = fpsMeasured;
				st.underruns = audioOK ? m_Audio.Underruns ()   : 0;
				st.overruns  = audioOK ? m_Audio.Overruns ()    : 0;
				st.queued    = audioOK ? m_Audio.QueuedFrames () : 0;
				st.target    = target;
				st.rom       = romPath;
				st.mode      = video_mode_file_value (m_Settings.video_mode);
				st.scale     = scale_name (m_Settings.scale_mode);
				m_Overlay.Draw (st);
			}

			m_Overlay.DrawToast ();   // transient toast, independent of HUD

			next += period_us;
			u64 t = CTimer::GetClockTicks64 ();
			if (next < t) next = t;
			while (CTimer::GetClockTicks64 () < next) { }

			if (audioOK)
			{
				// Sample the queue depth before gating, classify for metrics,
				// then apply the high-watermark gate.
				unsigned q = m_Audio.QueuedFrames ();
				switch (classify_queue (q, 0, target + framesPerVideo))
				{
				case AQ_Underrun: m_Audio.RecordUnderrun (); break;
				case AQ_Overrun:  m_Audio.RecordOverrun ();  break;
				default: break;
				}
				while (m_Audio.QueuedFrames () > target + framesPerVideo) { }
			}

			if (++frame >= 30)
			{
				frame = 0;
				ledOn = !ledOn;
				if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
			}

			if (++logCtr >= 300)   // ~5 s at 60 fps
			{
				logCtr = 0;
				if (audioOK)
				{
					m_Logger.Write (FromKernel, LogNotice,
						"audio underruns=%u overruns=%u",
						m_Audio.Underruns (), m_Audio.Overruns ());
				}
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
