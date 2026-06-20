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

// Per-port button caches, updated asynchronously by the USB report handlers.
// One aligned word each, read/written atomically. Static because Circle's
// RegisterStatusHandler takes a bare function pointer (no userdata).
static volatile unsigned s_buttons[Gamepad::MAX_PADS] = { 0, 0 };

// Fold a raw generic-HID gamepad report into our GP_* bitmask: raw digital
// buttons + 8-way hat + analog-axis D-pad. Shared by both pads' handlers.
static unsigned decode_buttons(const TGamePadState *pState)
{
    unsigned b = (unsigned) pState->buttons;   // raw HID digital buttons

    // D-pad as an 8-way hat (0..7) -> direction bits.
    int hat = pState->nhats > 0 ? pState->hats[0] : -1;
    switch (hat)
    {
    case 0: b |= GP_UP;              break;
    case 1: b |= GP_UP | GP_RIGHT;   break;
    case 2: b |= GP_RIGHT;           break;
    case 3: b |= GP_DOWN | GP_RIGHT; break;
    case 4: b |= GP_DOWN;            break;
    case 5: b |= GP_DOWN | GP_LEFT;  break;
    case 6: b |= GP_LEFT;            break;
    case 7: b |= GP_UP | GP_LEFT;    break;
    default: break;                  // centered (or no hat)
    }

    // D-pad as analog axes (X=axis0, Y=axis1): threshold about the midpoint
    // with a 25% deadzone.
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

    return b;
}

// One dedicated handler per pad slot (avoids relying on nDeviceIndex numbering).
static void Handler0(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[0] = decode_buttons(pState);
}

static void Handler1(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[1] = decode_buttons(pState);
}

Gamepad::Gamepad(CDeviceNameService *pNameService)
:   m_pNameService(pNameService)
{
    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        m_pDevice[i] = 0;
    }
}

boolean Gamepad::IsPresent(unsigned port) const
{
    return port < MAX_PADS && m_pDevice[port] != 0;
}

void Gamepad::Poll(void)
{
    static TGamePadStatusHandler *const handlers[MAX_PADS] = { Handler0, Handler1 };

    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        if (m_pDevice[i] == 0)   // plug-and-play: pads appear shortly after boot
        {
            m_pDevice[i] = (CUSBGamePadDevice *)
                m_pNameService->GetDevice("upad", i + 1, FALSE);
            if (m_pDevice[i] != 0)
            {
                // Circle only decodes reports while a handler is registered.
                m_pDevice[i]->RegisterStatusHandler(handlers[i]);
            }
        }
    }
}

unsigned Gamepad::Buttons(unsigned port) const
{
    return port < MAX_PADS ? s_buttons[port] : 0;
}

unsigned Gamepad::MenuButtons(void) const
{
    return s_buttons[0] | s_buttons[1];
}
