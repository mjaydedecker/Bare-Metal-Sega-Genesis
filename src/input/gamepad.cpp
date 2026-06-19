//
// src/input/gamepad.cpp
//
// Bare Metal Sega Genesis
// See gamepad.h.
//

#include "gamepad.h"
#include "joypad_map.h"

// The pure mapping mirrors Circle's TGamePadButton bits; verify they match so
// the host-tested constants can never drift from the driver.
static_assert(GP_A == GamePadButtonA,           "GP_A bit mismatch");
static_assert(GP_B == GamePadButtonB,           "GP_B bit mismatch");
static_assert(GP_X == GamePadButtonX,           "GP_X bit mismatch");
static_assert(GP_Y == GamePadButtonY,           "GP_Y bit mismatch");
static_assert(GP_LB == GamePadButtonLB,         "GP_LB bit mismatch");
static_assert(GP_RB == GamePadButtonRB,         "GP_RB bit mismatch");
static_assert(GP_START == GamePadButtonStart,   "GP_START bit mismatch");
static_assert(GP_SELECT == GamePadButtonSelect, "GP_SELECT bit mismatch");
static_assert(GP_UP == GamePadButtonUp,         "GP_UP bit mismatch");
static_assert(GP_DOWN == GamePadButtonDown,     "GP_DOWN bit mismatch");
static_assert(GP_LEFT == GamePadButtonLeft,     "GP_LEFT bit mismatch");
static_assert(GP_RIGHT == GamePadButtonRight,   "GP_RIGHT bit mismatch");

Gamepad::Gamepad(CDeviceNameService *pNameService)
:   m_pNameService(pNameService), m_pDevice(0), m_Buttons(0)
{
}

boolean Gamepad::Initialize(void)
{
    m_pDevice = (CUSBGamePadDevice *)
        m_pNameService->GetDevice("upad", 1, FALSE);
    return m_pDevice != 0;
}

boolean Gamepad::IsPresent(void) const
{
    return m_pDevice != 0;
}

void Gamepad::Poll(void)
{
    if (m_pDevice != 0)
    {
        const TGamePadState *pState = m_pDevice->GetReport();
        m_Buttons = pState != 0 ? (unsigned) pState->buttons : 0;
    }
    else
    {
        m_Buttons = 0;
    }
}

unsigned Gamepad::Buttons(void) const
{
    return m_Buttons;
}
