//
// src/menu/controls_screen.cpp
//
// Bare Metal Sega Genesis
// See controls_screen.h.
//

#include "controls_screen.h"
#include "../input/joypad_map.h"   // GP_* via header (GP_UP etc. for nav)
#include "../input/menu_input.h"   // menu_buttons (USB + GPIO)
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include <circle/timer.h>

#define NUM_BTN  8                 // Genesis action buttons
#define NUM_ROWS (NUM_BTN + 1)     // + the Player selector row

static const char *const GEN_LABEL[NUM_BTN] =
    { "A", "B", "C", "X", "Y", "Z", "Start", "Mode" };

static const char *phys_label(PadButton p)
{
    switch (p)
    {
    case PadButton::A:      return "A";
    case PadButton::B:      return "B";
    case PadButton::X:      return "X";
    case PadButton::Y:      return "Y";
    case PadButton::L:      return "L";
    case PadButton::R:      return "R";
    case PadButton::Start:  return "Start";
    case PadButton::Select: return "Select";
    }
    return "?";
}

ControlsScreen::ControlsScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore)
{
}

void ControlsScreen::Render(int player, int selected)
{
    using namespace chrome;
    const ButtonMap &map = (player == 0) ? m_pSettings->map1 : m_pSettings->map2;

    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "CONTROLS", player == 0 ? "Player 1" : "Player 2",
           theme::VALUE);

    // Row 0: player selector (adjustable). Rows 1..8: button maps (adjustable).
    value_row(m_pCanvas, 0, selected == 0, "Player",
              player == 0 ? "1" : "2", theme::VALUE, true);
    for (int b = 0; b < NUM_BTN; b++)
    {
        value_row(m_pCanvas, b + 1, selected == b + 1, GEN_LABEL[b],
                  phys_label(map.b[b]), theme::VALUE, true);
    }

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_dpad(m_pCanvas, PAD, fy, "NAVIGATE");
    hx = hint_lr(m_pCanvas, hx, fy, "REMAP");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
}

void ControlsScreen::Run(void)
{
    int player   = 0;
    int selected = 0;
    Render(player, selected);

    unsigned prev = menu_buttons(m_pGamepad);
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = menu_buttons(m_pGamepad);
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)
        {
            selected = (selected + NUM_ROWS - 1) % NUM_ROWS;
            Render(player, selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = (selected + 1) % NUM_ROWS;
            Render(player, selected);
        }

        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            if (selected == 0)
            {
                player ^= 1;                     // toggle P1/P2
            }
            else
            {
                ButtonMap &map = (player == 0) ? m_pSettings->map1
                                               : m_pSettings->map2;
                int btn = selected - 1;
                int v = (int) map.b[btn] + dir;
                if (v < 0) v = 7;
                if (v > 7) v = 0;
                map.b[btn] = (PadButton) v;
                m_pStore->Save(*m_pSettings);
            }
            Render(player, selected);
        }

        if (pressed & GP_B)
        {
            return;
        }

        CTimer::SimpleMsDelay(16);
    }
}
