//
// kernel.h
//
// Bare Metal Sega Genesis
// CKernel — top-level orchestrator for the bare-metal emulator.
//
// M1 stub: initialises core Circle subsystems only.
// Emulation components are wired in during subsequent milestones.
//

#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/logger.h>
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
	CLogger            m_Logger;
};

#endif
