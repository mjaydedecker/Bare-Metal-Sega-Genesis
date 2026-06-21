//
// src/menu/hotkey_screen.h
//
// Bare Metal Sega Genesis
// In-game hotkey remap screen: per action, cycle the hold + trigger buttons.
// Reached from the Settings screen. Mirrors ControlsScreen.
//

#ifndef _menu_hotkey_screen_h
#define _menu_hotkey_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"

class HotkeyScreen
{
public:
    HotkeyScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                 Settings *pSettings, SettingsStore *pStore);
    void Run(void);   // returns when the user backs out (B)

private:
    void Render(int selected);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
};

#endif
