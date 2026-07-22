//
// kernel.cpp
//
// Bare Metal Sega Genesis
//

#include "kernel.h"
#include "input/joypad_map.h"   // GP_START, GP_SELECT bits for the menu hotkey
#include "input/hotkey.h"       // decode_hotkey, InGameAction
#include "input/input_merge.h"  // merge_buttons (USB + GPIO coexist)
#include "input/menu_input.h"   // menu_buttons (USB + GPIO, menu navigation)
#include "input/pad_device.h"   // pad_use_6button (per-port MD pad type)
#include "input/pad_toast.h"    // pad_toast_label (GPIO pad detection toast)
#include "audio/audio_util.h"   // classify_queue, AQ_* for metrics
#include "video/splash.h"       // splash_show_embedded, splash_apply_override
#include "ui/theme.h"
#include "ui/fonts/font_ps2p8.h"
#include "ui/fonts/font_vt323_22.h"

static const char FromKernel[] = "kernel";

// Genesis-Plus-GX controller device subclasses (3- and 6-button MD pad). At file
// scope so both ROM-load and PadDeviceFor() can choose a per-port device.
#define RETRO_DEVICE_MDPAD_6B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_MDPAD_3B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)

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
	m_GlyphCanvas (&m_Display),
	m_Overlay (&m_GlyphCanvas),
	m_RomMenu (&m_GlyphCanvas, &m_Gamepad, &m_Storage, &m_USBHCI),
	m_Settings (),
	m_SettingsStore (&m_Storage),
	m_VideoModeScreen (&m_GlyphCanvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display),
	m_ControlsScreen (&m_GlyphCanvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore),
	m_HotkeyScreen (&m_GlyphCanvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore),
	m_ControllerStore (),
	m_CalibrationScreen (&m_GlyphCanvas, &m_Gamepad, &m_USBHCI, &m_ControllerStore, &m_Storage),
	m_SettingsScreen (&m_GlyphCanvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display, &m_Audio, &m_ControlsScreen, &m_VideoModeScreen, &m_Overlay, &m_HotkeyScreen, &m_CalibrationScreen),
	m_PauseMenu (&m_GlyphCanvas, &m_Gamepad, &m_USBHCI, &m_SaveState, &m_SettingsScreen),
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
		m_Logger.Write (FromKernel, LogNotice, "Initialising video");
		bOK = m_Display.Initialize ();
		if (!bOK)
		{
			m_Logger.Write (FromKernel, LogPanic, "Display init failed");
		}
		else
		{
			// Branded splash up ASAP, masking the USB/SD init that follows.
			splash_show_text (&m_GlyphCanvas, &m_Display);
		}
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

unsigned CKernel::MergedMenuButtons (void)
{
	return menu_buttons (&m_Gamepad);
}

unsigned CKernel::PadDeviceFor (unsigned port)
{
	return pad_use_6button (m_GpioPads.PadTypeAt (port), m_Settings.pad_type)
	     ? RETRO_DEVICE_MDPAD_6B : RETRO_DEVICE_MDPAD_3B;
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

	// Replace the embedded logo with SD:/splash.raw if the user supplied one.
	splash_apply_override (&m_Storage, &m_Canvas, &m_Display);

	// Load per-controller calibrations so Poll() picks them on pad acquire.
	m_ControllerStore.Load (&m_Storage);
	m_Gamepad.SetCalibrations (m_ControllerStore.Data (), m_ControllerStore.Count ());

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

	m_GpioPads.Init ();          // configure DB9 GPIO pins (per sega_board.h)
	g_gpio_pads = &m_GpioPads;   // input_state_cb ORs these into the per-port mask

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
			m_GlyphCanvas.Clear (theme::BG);
			m_GlyphCanvas.Text (&g_font_ps2p8, 2, 40, 40, "FAILED TO READ ROM",
				0xF800, theme::BG, true);
			m_GlyphCanvas.Text (&g_font_vt323_22, 1, 40, 80,
				"Returning to browser...", theme::TEXT, theme::BG, true);
			m_GlyphCanvas.Scanlines (0, 0, (int) m_GlyphCanvas.Width (),
				(int) m_GlyphCanvas.Height (), 60);
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
			m_GlyphCanvas.Clear (theme::BG);
			m_GlyphCanvas.Text (&g_font_ps2p8, 2, 40, 40, "FAILED TO LOAD ROM",
				0xF800, theme::BG, true);
			m_GlyphCanvas.Text (&g_font_vt323_22, 1, 40, 80,
				"Returning to browser...", theme::TEXT, theme::BG, true);
			m_GlyphCanvas.Scanlines (0, 0, (int) m_GlyphCanvas.Width (),
				(int) m_GlyphCanvas.Height (), 60);
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
		u64      period_us      = (u64) (1000000.0 / fps);

		// Audio: initialise once (Genesis sample rate is constant).
		if (!audioInited && sampleRate > 0 && m_Audio.Initialize (sampleRate, m_Settings.audio_output))
		{
			audioInited = TRUE;
			g_audio = &m_Audio;
		}
		boolean audioOK = audioInited;

		m_GpioPads.Poll ();   // fresh pad-type read before choosing port devices
		retro_set_controller_port_device (0, PadDeviceFor (0));
		retro_set_controller_port_device (1, PadDeviceFor (1));

		// Surface any detected GPIO pad so the user knows it's live.
		for (unsigned p = 0; p < GpioPads::NUM_PORTS; ++p)
		{
			if (!m_GpioPads.IsPresent (p))
				continue;
			char msg[20];
			pad_toast_label (msg, sizeof msg, p, m_GpioPads.PadTypeAt (p));
			m_Overlay.ShowToast (msg, TOAST_SUCCESS);
		}

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
		boolean  ledOn     = FALSE;
		unsigned prevBtns  = 0;
		unsigned prevP1    = 0;   // player-1 button state for in-game hotkeys
		boolean  toBrowser = FALSE;

		for (;;)
		{
			m_USBHCI.UpdatePlugAndPlay ();
			m_Gamepad.Poll ();
			m_GpioPads.Poll ();   // one SELECT burst/frame (respects 1.5ms reset)
			unsigned now     = MergedMenuButtons ();
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
				prevBtns = MergedMenuButtons ();           // resync after the menu
				next     = CTimer::GetClockTicks64 ();    // re-baseline pacing
				if (toBrowser) break;
				m_Display.ForceRepaint ();   // wipe the overlay from the letterbox bars
				continue;
			}

			// In-game action hotkeys (player 1: Select + button), read live.
			unsigned     p1now     = merge_buttons (m_Gamepad.Buttons (0),
			                                        m_GpioPads.Buttons (0));
			unsigned     p1pressed = p1now & ~prevP1;
			prevP1 = p1now;
			InGameAction act = decode_hotkey (p1now, p1pressed, m_Settings.hotkeys);
			switch (act)
			{
			case InGameAction::QuickSave:
			{
				bool ok = m_SaveState.Save (1);
				m_Overlay.ShowToast (ok ? "Quick-saved" : "Save failed",
				                     ok ? TOAST_SUCCESS : TOAST_FAIL);
				break;
			}
			case InGameAction::QuickLoad:
				if (!m_SaveState.Occupied (1))
					m_Overlay.ShowToast ("No quick save", TOAST_FAIL);
				else
				{
					bool ok = m_SaveState.Load (1);
					m_Overlay.ShowToast (ok ? "Quick-loaded" : "Load failed",
					                     ok ? TOAST_SUCCESS : TOAST_FAIL);
				}
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
				m_Overlay.ShowToast (t, TOAST_INFO);
				break;
			}
			case InGameAction::ToggleHud:
				m_Settings.debug_overlay = !m_Settings.debug_overlay;
				m_Overlay.SetEnabled (m_Settings.debug_overlay);
				m_SettingsStore.Save (m_Settings);
				m_Overlay.ShowToast (m_Settings.debug_overlay ? "HUD on" : "HUD off",
				                     TOAST_INFO);
				if (!m_Settings.debug_overlay) m_Display.ForceRepaint ();
				break;
			case InGameAction::Mute:
				m_Settings.mute = !m_Settings.mute;
				m_Audio.SetMute (m_Settings.mute);
				m_SettingsStore.Save (m_Settings);
				m_Overlay.ShowToast (m_Settings.mute ? "Muted" : "Unmuted", TOAST_INFO);
				break;
			case InGameAction::None:
				break;
			}

			retro_run ();
			m_Sram.Tick ();              // periodic dirty-checked SRAM auto-save

			// Audio buffering depth from the live latency setting (Medium == the
			// historical framesPerVideo*2). The gate stays one frame above target.
			unsigned target = audio_latency_frames (m_Settings.audio_latency)
			                  * framesPerVideo;

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
				st.vsync      = m_Settings.vsync;
				st.widescreen = m_Settings.widescreen;
				st.latency    = audio_latency_file_value (m_Settings.audio_latency);
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

			// (No periodic on-screen logging here: kernel logs go to the screen
			// device, which is not vsync-page-aware, so writing mid-game smeared
			// console text into the letterbox margins. Audio underruns/overruns
			// are already shown by the diagnostics HUD/overlay.)
		}

		// --- Unload and return to the browser ---
		m_Sram.Save ();                  // flush battery SRAM before unloading
		retro_unload_game ();
		delete[] m_pROMBuffer;
		m_pROMBuffer = 0;
	}

	return ShutdownHalt;
}
