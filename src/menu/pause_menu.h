//
// src/menu/pause_menu.h
//
// Bare Metal Sega Genesis
// In-emulation overlay menu. Pauses the caller; returns the chosen action.
//

#ifndef _menu_pause_menu_h
#define _menu_pause_menu_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "save_state.h"

enum class MenuAction { Resume, Reset, ReturnToBrowser };

class PauseMenu
{
public:
    PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
              SaveState *pSaveState);
    MenuAction Run(void);     // draws over the last frame; returns on confirm

private:
    void Render(int selected);
    int  PickSlot(bool forLoad);   // returns slot 1..4, or 0 to cancel
    void Message(const char *text);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    SaveState     *m_pSaveState;
};

#endif
