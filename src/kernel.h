//
// kernel.h
//
// Bare Metal Sega Genesis
// CKernel — top-level orchestrator for the bare-metal emulator.
//
// M2: all Circle subsystems required by the emulator are declared here.
// Emulation components are wired in during subsequent milestones.
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
	CUSBHCIDevice      m_USBHCI;    // gamepad input (M7)
	CEMMCDevice        m_EMMC;      // SD card block device (M3)
	CFATFileSystem     m_FileSystem; // FAT filesystem over EMMC (M3)
	CPWMSoundDevice    m_Sound;     // PWM audio output (M6)
};

#endif
