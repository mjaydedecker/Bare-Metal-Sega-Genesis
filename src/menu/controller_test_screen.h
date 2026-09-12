//
// src/menu/controller_test_screen.h
//
// Bare Metal Sega Genesis
// Live controller tester reached from Settings: four panels (HAT DB9 ports 1-2,
// USB pads 1-2) showing presence, pad type and a Genesis pad drawing whose
// buttons light while held. Hold Start ~2 s on any pad to leave. Pure logic
// lives in pad_test.
//

#ifndef _menu_controller_test_screen_h
#define _menu_controller_test_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/glyph_canvas.h"
#include "../input/gamepad.h"
#include "../input/gpio_pads.h"

class ControllerTestScreen
{
public:
    ControllerTestScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                         CUSBHCIDevice *pUSBHCI, GpioPads *pGpioPads);
    void Run(void);

    // One panel's inputs; compared between loops to redraw only on change.
    struct PanelState
    {
        bool     present;
        uint8_t  kind;      // HAT: SegaPadType value; USB: 0
        unsigned buttons;   // GP_* mask
        unsigned vid, pid;  // USB only
    };

private:
    void Sample(PanelState s[4]);
    void Render(const PanelState s[4]);
    void RenderPanel(int index, const PanelState &s, int x, int y, int w, int h);
    void RenderProgress(unsigned pct);

    GlyphCanvas   *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    GpioPads      *m_pGpioPads;
    int            m_BarX, m_BarY;   // footer hold-progress bar position
};

#endif
