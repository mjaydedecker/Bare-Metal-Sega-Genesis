//
// src/input/gamepad.cpp
//
// Bare Metal Sega Genesis
// See gamepad.h.
//

#include "gamepad.h"
#include "joypad_map.h"
#include "pad_reconcile.h"
#include <circle/usb/usbdevice.h>
#include <circle/usb/usb.h>

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

// Raw HID button word per port (pre-calibration), and the active per-port
// calibration + cached USB ids. Static because the report handlers take no
// userdata (same reason as s_buttons).
static volatile unsigned    s_raw[Gamepad::MAX_PADS] = { 0, 0 };
static const ControllerCal *s_cal[Gamepad::MAX_PADS] = { 0, 0 };
static unsigned             s_vid[Gamepad::MAX_PADS] = { 0, 0 };
static unsigned             s_pid[Gamepad::MAX_PADS] = { 0, 0 };

// Fold a raw generic-HID gamepad report into our GP_* bitmask: raw digital
// buttons + 8-way hat + analog-axis D-pad. Shared by both pads' handlers.
static unsigned decode_buttons(unsigned slot, const TGamePadState *pState)
{
    unsigned raw = (unsigned) pState->buttons;   // raw HID digital buttons
    s_raw[slot] = raw;
    // Calibrated pad: remap raw bits to GP_* slots. Uncalibrated: passthrough
    // (today's 8BitDo identity behavior, unchanged).
    unsigned b = s_cal[slot] ? controller_decode(raw, *s_cal[slot]) : raw;

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
    s_buttons[0] = decode_buttons(0, pState);
}

static void Handler1(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[1] = decode_buttons(1, pState);
}

Gamepad::Gamepad(CDeviceNameService *pNameService)
:   m_pNameService(pNameService), m_pCal(0), m_nCal(0)
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
        CUSBGamePadDevice *dev = (CUSBGamePadDevice *)
            m_pNameService->GetDevice("upad", i + 1, FALSE);

        switch (pad_reconcile(m_pDevice[i], dev))
        {
        case PadAction::Clear:           // unplugged: stop ghost inputs, free slot
            s_buttons[i] = 0;
            s_raw[i] = 0;
            s_cal[i] = 0;
            s_vid[i] = s_pid[i] = 0;
            m_pDevice[i] = 0;
            break;

        case PadAction::Acquire:         // first plug / re-plug / swap
        {
            m_pDevice[i] = dev;
            s_buttons[i] = 0;
            s_raw[i] = 0;

            u16 vid = 0, pid = 0;
            CUSBDevice *udev = dev->GetDevice();
            if (udev != 0)
            {
                const TUSBDeviceDescriptor *d = udev->GetDeviceDescriptor();
                if (d != 0) { vid = d->idVendor; pid = d->idProduct; }
            }
            s_vid[i] = vid;
            s_pid[i] = pid;
            s_cal[i] = (m_pCal && m_nCal)
                       ? controller_find(m_pCal, m_nCal, vid, pid) : 0;

            // Circle only decodes reports while a handler is registered.
            dev->RegisterStatusHandler(handlers[i]);
            break;
        }

        case PadAction::Keep:            // same device, or still absent
            break;
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

unsigned Gamepad::RawButtons(unsigned port) const
{
    return port < MAX_PADS ? s_raw[port] : 0;
}

unsigned Gamepad::VendorId(unsigned port) const
{
    return port < MAX_PADS ? s_vid[port] : 0;
}

unsigned Gamepad::ProductId(unsigned port) const
{
    return port < MAX_PADS ? s_pid[port] : 0;
}

void Gamepad::SetCalibrations(const ControllerCal *arr, unsigned count)
{
    m_pCal = arr;
    m_nCal = count;
}

void Gamepad::ReacquireCal(unsigned port)
{
    if (port < MAX_PADS)
        s_cal[port] = (m_pCal && m_nCal)
                      ? controller_find(m_pCal, m_nCal, s_vid[port], s_pid[port]) : 0;
}
