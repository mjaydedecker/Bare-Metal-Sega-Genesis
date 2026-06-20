//
// src/menu/settings_screen.h
//
// Bare Metal Sega Genesis
// In-emulation Settings screen. Edits the shared Settings, applies changes
// (scale mode live via Display; widescreen via the environment globals), and
// persists to SD:/settings.txt through SettingsStore.
//

#ifndef _menu_settings_screen_h
#define _menu_settings_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"
#include "../video/display.h"

class SettingsScreen
{
public:
    SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay);
    void Run(void);   // returns when the user backs out (B)

private:
    void Render(int selected);
    void Apply(void);   // push current settings to Display + env globals

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
    Display       *m_pDisplay;
};

#endif
