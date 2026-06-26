//
// kernel.h
//
// Bare Metal Sega Genesis
// CKernel — top-level orchestrator for the bare-metal emulator.
//

#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/cputhrottle.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include "audio/audio_driver.h"
#include <SDCard/emmc.h>
#include <circle/types.h>
#include "storage/storage.h"
#include "menu/rom_menu.h"
#include "libretro/environment.h"
#include "libretro/callbacks.h"
#include "video/display.h"
#include "ui/text_canvas.h"
#include "ui/glyph_canvas.h"
#include "ui/overlay.h"
#include "menu/save_state.h"
#include "menu/sram.h"
#include "settings/settings.h"
#include "settings/settings_store.h"
#include "menu/settings_screen.h"
#include "menu/controls_screen.h"
#include "menu/hotkey_screen.h"
#include "menu/video_mode_screen.h"
#include "menu/calibration_screen.h"
#include "menu/pause_menu.h"
#include "input/gamepad.h"
#include "input/gpio_pads.h"
#include "input/controller_store.h"
#include <libretro.h>

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);
	TShutdownMode Run (void);

private:
	// Initialisation order matters — do not reorder these members.
	CActLED            m_ActLED;
	CKernelOptions     m_Options;
	// CPU clock: raise to maximum early, but AFTER m_Options — CCPUThrottle's
	// constructor calls CKernelOptions::Get(), which must already exist.
	CCPUThrottle       m_CPUThrottle;
	CDeviceNameService m_DeviceNameService;
	CScreenDevice      m_Screen;
	CSerialDevice      m_Serial;
	CExceptionHandler  m_ExceptionHandler;
	CInterruptSystem   m_Interrupt;
	CTimer             m_Timer;
	CLogger            m_Logger;
	CUSBHCIDevice      m_USBHCI;     // gamepad input (M7)
	CEMMCDevice        m_EMMC;       // SD card block device
	AudioDriver        m_Audio;      // HDMI audio output (M6)
	Display            m_Display;    // HDMI video output (M5)
	Gamepad            m_Gamepad;    // USB controller input (M7)
	GpioPads           m_GpioPads;   // real Sega DB9 pads on GPIO (coexist w/ USB)
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	SaveState          m_SaveState;  // save/load core state to SD slots
	Sram               m_Sram;       // battery SRAM persistence
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	GlyphCanvas        m_GlyphCanvas;// redesigned-screen renderer (pilot: ROM browser)
	Overlay            m_Overlay;    // diagnostics HUD over the game frame
	RomMenu            m_RomMenu;    // on-screen ROM browser
	Settings           m_Settings;       // user settings (video, etc.)
	SettingsStore      m_SettingsStore;  // load/save settings to SD card
	VideoModeScreen    m_VideoModeScreen; // HDMI mode picker
	ControlsScreen     m_ControlsScreen; // button-remapping sub-screen
	HotkeyScreen       m_HotkeyScreen;   // in-game hotkey remap sub-screen
	ControllerStore    m_ControllerStore; // per-VID/PID calibrations (SD)
	CalibrationScreen  m_CalibrationScreen; // press-each-button calibration
	SettingsScreen     m_SettingsScreen; // in-emulation Settings screen
	PauseMenu          m_PauseMenu;  // in-emulation overlay menu

	// Menu-navigation button state OR'd across the USB pad and both GPIO ports,
	// so either input source can drive the pause menu / hotkeys.
	unsigned MergedMenuButtons (void);

	// MDPAD device (3B/6B) for a port: from live GPIO detection if a Sega pad is
	// present, else the global pad_type setting (USB pads).
	unsigned PadDeviceFor (unsigned port);

	// ROM buffer — allocated by SDCard::ReadFile, passed to core in M4.
	u8    *m_pROMBuffer;
	size_t m_nROMSize;
};

#endif
