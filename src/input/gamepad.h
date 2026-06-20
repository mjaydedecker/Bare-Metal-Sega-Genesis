//
// src/input/gamepad.h
//
// Bare Metal Sega Genesis
// Wraps Circle's generic USB gamepad. Circle only decodes gamepad reports when
// a status handler is registered, so we register one that caches the button
// bitmask; the kernel reads it via Buttons(). Knows nothing about libretro.
//

#ifndef _input_gamepad_h
#define _input_gamepad_h

#include <circle/devicenameservice.h>
#include <circle/usb/usbgamepad.h>
#include <circle/types.h>

class Gamepad
{
public:
    Gamepad(CDeviceNameService *pNameService);

    boolean  IsPresent(void) const;
    void     Poll(void);           // acquire "upad1" + register handler when it appears
    unsigned Buttons(void) const;  // latest button bitmask (handler-updated)

private:
    CDeviceNameService *m_pNameService;
    CUSBGamePadDevice  *m_pDevice;
};

#endif
