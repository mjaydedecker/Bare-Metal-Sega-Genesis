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
#include "input/gamepad.h"
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
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	RomMenu            m_RomMenu;    // on-screen ROM browser

	// ROM buffer — allocated by SDCard::ReadFile, passed to core in M4.
	u8    *m_pROMBuffer;
	size_t m_nROMSize;
};

#endif
