//
// src/menu/controls_screen.h
//
// Bare Metal Sega Genesis
// Button-remapping sub-screen: pick a player and reassign each Genesis action
// button to a physical pad button. Reached from the Settings screen.
//

#ifndef _menu_controls_screen_h
#define _menu_controls_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"

class ControlsScreen
{
public:
    ControlsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore);
    void Run(void);   // returns when the user backs out (B)

private:
    void Render(int player, int selected);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
};

#endif
