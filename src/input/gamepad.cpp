//
// src/input/gamepad.cpp
//
// Bare Metal Sega Genesis
// See gamepad.h.
//

#include "gamepad.h"
#include "joypad_map.h"

// Direction-bit constants mirror Circle's TGamePadButton (host-tested in
// joypad_map). We fold the hat into these.
static_assert(GP_UP == GamePadButtonUp,       "GP_UP bit mismatch");
static_assert(GP_DOWN == GamePadButtonDown,   "GP_DOWN bit mismatch");
static_assert(GP_LEFT == GamePadButtonLeft,   "GP_LEFT bit mismatch");
static_assert(GP_RIGHT == GamePadButtonRight, "GP_RIGHT bit mismatch");

// Updated asynchronously by the USB driver's report handler. Single player, so
// a file-static cache is fine; one aligned word is read/written atomically.
static volatile unsigned s_buttons = 0;

static void GamepadStatusHandler(unsigned /*nDeviceIndex*/,
                                 const TGamePadState *pState)
{
    unsigned b = (unsigned) pState->buttons;   // raw HID digital buttons

    // Generic HID pads report the D-pad as an 8-way hat (0..7), not as the
    // normalized direction bits. Fold the hat into direction bits.
    int hat = pState->nhats > 0 ? pState->hats[0] : -1;
    switch (hat)
    {
    case 0: b |= GP_UP;            break;
    case 1: b |= GP_UP | GP_RIGHT; break;
    case 2: b |= GP_RIGHT;         break;
    case 3: b |= GP_DOWN | GP_RIGHT; break;
    case 4: b |= GP_DOWN;          break;
    case 5: b |= GP_DOWN | GP_LEFT; break;
    case 6: b |= GP_LEFT;          break;
    case 7: b |= GP_UP | GP_LEFT;  break;
    default: break;                // centered (or no hat)
    }

    // D-pad as analog axes: many pads (incl. this one) report the D-pad on the
    // first two axes (X=axis0, Y=axis1, ~center..extremes). Threshold each axis
    // about its midpoint with a 25% deadzone.
    if (pState->naxes >= 2)
    {
        int xmin = pState->axes[0].minimum, xmax = pState->axes[0].maximum;
        int ymin = pState->axes[1].minimum, ymax = pState->axes[1].maximum;
        if (xmax > xmin)
        {
            int mid = (xmin + xmax) / 2, dz = (xmax - xmin) / 4;
            int v = pState->axes[0].value;
            if      (v < mid - dz) b |= GP_LEFT;
            else if (v > mid + dz) b |= GP_RIGHT;
        }
        if (ymax > ymin)
        {
            int mid = (ymin + ymax) / 2, dz = (ymax - ymin) / 4;
            int v = pState->axes[1].value;
            if      (v < mid - dz) b |= GP_UP;
            else if (v > mid + dz) b |= GP_DOWN;
        }
    }

    s_buttons = b;
}

Gamepad::Gamepad(CDeviceNameService *pNameService)
:   m_pNameService(pNameService), m_pDevice(0)
{
}

boolean Gamepad::IsPresent(void) const
{
    return m_pDevice != 0;
}

void Gamepad::Poll(void)
{
    if (m_pDevice == 0)   // plug-and-play: the pad appears shortly after boot
    {
        m_pDevice = (CUSBGamePadDevice *)
            m_pNameService->GetDevice("upad", 1, FALSE);
        if (m_pDevice != 0)
        {
            // Circle only decodes reports (updates the state) while a status
            // handler is registered; without this the state is always 0.
            m_pDevice->RegisterStatusHandler(GamepadStatusHandler);
        }
    }
}

unsigned Gamepad::Buttons(void) const { return s_buttons; }
