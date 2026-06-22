//
// src/menu/video_mode_screen.h
//
// Bare Metal Sega Genesis
// HDMI output-mode picker with a confirm-or-revert guard: applying a mode the
// TV can't display auto-reverts, and only a confirmed mode is persisted.
//

#ifndef _menu_video_mode_screen_h
#define _menu_video_mode_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/glyph_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"
#include "../video/display.h"

class VideoModeScreen
{
public:
    VideoModeScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                    Settings *pSettings, SettingsStore *pStore, Display *pDisplay);
    void Run(void);   // returns when the user backs out (B)

private:
    void    Render(VideoMode sel);
    void    Apply(VideoMode want);   // SetMode + confirm-or-revert + maybe save
    boolean Confirm(void);           // ~15s countdown; A=keep, B/timeout=revert

    GlyphCanvas   *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
    Display       *m_pDisplay;
};

#endif
