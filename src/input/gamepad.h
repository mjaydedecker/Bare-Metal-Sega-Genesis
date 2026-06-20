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
    static const unsigned MAX_PADS = 2;   // Player 1 + Player 2

    Gamepad(CDeviceNameService *pNameService);

    // Acquire upad1/upad2 and register each pad's report handler (lazy
    // plug-and-play; pads enumerate a moment after boot).
    void Poll(void);

    // Latest button bitmask for a port (0 if the port has no pad / is invalid).
    unsigned Buttons(unsigned port = 0) const;

    // Combined bitmask of both pads — used for menu navigation / pause hotkey
    // so either controller can drive the UI.
    unsigned MenuButtons(void) const;

    boolean IsPresent(unsigned port = 0) const;

private:
    CDeviceNameService *m_pNameService;
    CUSBGamePadDevice  *m_pDevice[MAX_PADS];
};

#endif
