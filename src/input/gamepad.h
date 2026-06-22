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
#include "controller_map.h"

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

    // Raw (pre-calibration) HID button word for a port — for the calibration screen.
    unsigned RawButtons(unsigned port = 0) const;
    // USB identifiers of the pad on a port (0 if absent).
    unsigned VendorId(unsigned port = 0) const;
    unsigned ProductId(unsigned port = 0) const;

    // Point the decoder at the loaded per-controller calibrations.
    void SetCalibrations(const ControllerCal *arr, unsigned count);
    // Re-pick a port's active calibration from its cached VID/PID (after a save).
    void ReacquireCal(unsigned port);

private:
    CDeviceNameService *m_pNameService;
    CUSBGamePadDevice  *m_pDevice[MAX_PADS];
    const ControllerCal *m_pCal;
    unsigned             m_nCal;
};

#endif
