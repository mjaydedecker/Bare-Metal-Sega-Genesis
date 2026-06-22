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
#include "../ui/glyph_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"
#include "../video/display.h"
#include "../audio/audio_driver.h"
#include "../ui/overlay.h"
#include "hotkey_screen.h"

class ControlsScreen;
class VideoModeScreen;
class CalibrationScreen;

class SettingsScreen
{
public:
    SettingsScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay,
                   AudioDriver *pAudio, ControlsScreen *pControls,
                   VideoModeScreen *pVideoMode, Overlay *pOverlay,
                   HotkeyScreen *pHotkey, CalibrationScreen *pCalibration);
    void Run(void);   // returns when the user backs out (B)

    // Tell the screen which ROM is currently running (for the auto-launch row).
    void SetCurrentRom(const char *path) { m_pRomPath = path; }

private:
    void Render(int selected);
    void Apply(void);   // push current settings to Display + env globals

    GlyphCanvas   *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
    Display       *m_pDisplay;
    AudioDriver   *m_pAudio;
    const char    *m_pRomPath;
    ControlsScreen *m_pControls;
    VideoModeScreen *m_pVideoMode;
    Overlay         *m_pOverlay;
    HotkeyScreen    *m_pHotkey;
    CalibrationScreen *m_pCalibration;
};

#endif
