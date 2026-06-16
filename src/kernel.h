//
// kernel.h
//
// Bare Metal Sega Genesis
// CKernel — top-level orchestrator for the bare-metal emulator.
//

#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/sound/pwmsounddevice.h>
#include <SDCard/emmc.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/types.h>
#include "storage/sdcard.h"
#include "libretro/environment.h"
#include "libretro/callbacks.h"
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
	CDeviceNameService m_DeviceNameService;
	CScreenDevice      m_Screen;
	CSerialDevice      m_Serial;
	CExceptionHandler  m_ExceptionHandler;
	CInterruptSystem   m_Interrupt;
	CTimer             m_Timer;
	CLogger            m_Logger;
	CUSBHCIDevice      m_USBHCI;     // gamepad input (M7)
	CEMMCDevice        m_EMMC;       // SD card block device
	CFATFileSystem     m_FileSystem; // FAT filesystem over EMMC
	CPWMSoundDevice    m_Sound;      // PWM audio output (M6)

	// Storage wrapper — declared after m_FileSystem and m_DeviceNameService.
	SDCard             m_SDCard;

	// ROM buffer — allocated by SDCard::ReadFile, passed to core in M4.
	u8    *m_pROMBuffer;
	size_t m_nROMSize;
};

#endif
