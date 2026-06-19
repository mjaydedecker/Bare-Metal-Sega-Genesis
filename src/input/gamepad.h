//
// src/input/gamepad.h
//
// Bare Metal Sega Genesis
// Wraps Circle's generic USB gamepad: find it, poll its normalized button
// bitmask. Knows nothing about libretro.
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

    boolean  Initialize(void);     // find "upad1"; FALSE if no pad present
    boolean  IsPresent(void) const;
    void     Poll(void);           // snapshot GetReport()->buttons
    unsigned Buttons(void) const;  // cached TGamePadButton bitmask (0 if none)

private:
    CDeviceNameService *m_pNameService;
    CUSBGamePadDevice  *m_pDevice;
    unsigned            m_Buttons;
};

#endif
