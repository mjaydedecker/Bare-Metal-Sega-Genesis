//
// src/menu/calibration_screen.h
//
// Bare Metal Sega Genesis
// Press-each-button calibration for the port-1 controller; saves a per-VID/PID
// mapping to the ControllerStore. Reached from the Settings screen.
//

#ifndef _menu_calibration_screen_h
#define _menu_calibration_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/glyph_canvas.h"
#include "../input/gamepad.h"
#include "../input/controller_store.h"
#include "../storage/storage.h"

class CalibrationScreen
{
public:
    CalibrationScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                      CUSBHCIDevice *pUSBHCI, ControllerStore *pStore,
                      Storage *pStorage);
    void Run(void);

private:
    void Prompt(unsigned vid, unsigned pid, int idx);
    void Message(const char *text);

    GlyphCanvas     *m_pCanvas;
    Gamepad         *m_pGamepad;
    CUSBHCIDevice   *m_pUSBHCI;
    ControllerStore *m_pStore;
    Storage         *m_pStorage;
};

#endif
