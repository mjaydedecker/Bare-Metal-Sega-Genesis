//
// src/menu/controls_screen.cpp
//
// Bare Metal Sega Genesis
// See controls_screen.h.
//

#include "controls_screen.h"
#include "../input/joypad_map.h"   // GP_* via header (GP_UP etc. for nav)
#include <circle/timer.h>

#define NUM_BTN  8                 // Genesis action buttons
#define NUM_ROWS (NUM_BTN + 1)     // + the Player selector row

static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

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

ControlsScreen::ControlsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore)
{
}

void ControlsScreen::Render(int player, int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2;
    int boxW = cw * 34, boxH = ch * (NUM_ROWS + 4);

    const ButtonMap &map = (player == 0) ? m_pSettings->map1 : m_pSettings->map2;

    m_pCanvas->Clear(0x0000);   // wipe any larger menu drawn before this one
    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "CONTROLS", WHITE, BOX);

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  ty  = boxY + ch * (i + 3);
        bool sel = (i == selected);
        u16  fg  = sel ? SELFG : WHITE;
        u16  bg  = sel ? SELBG : BOX;
        if (sel) m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        m_pCanvas->DrawText(boxX + cw, ty, sel ? ">" : " ", fg, bg);

        if (i == 0)
        {
            m_pCanvas->DrawText(boxX + cw * 3,  ty, "Player", fg, bg);
            m_pCanvas->DrawText(boxX + cw * 13, ty, player == 0 ? "< 1 >" : "< 2 >",
                                fg, bg);
        }
        else
        {
            int btn = i - 1;
            char val[12];
            val[0] = '<'; val[1] = ' ';
            const char *pl = phys_label(map.b[btn]);
            int k = 2;
            for (int j = 0; pl[j] && k < 9; j++) val[k++] = pl[j];
            val[k++] = ' '; val[k++] = '>'; val[k] = '\0';
            m_pCanvas->DrawText(boxX + cw * 3,  ty, GEN_LABEL[btn], fg, bg);
            m_pCanvas->DrawText(boxX + cw * 13, ty, val, fg, bg);
        }
    }

    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 3),
                        "Left/Right: change   B: back", WHITE, BOX);
}

void ControlsScreen::Run(void)
{
    int player   = 0;
    int selected = 0;
    Render(player, selected);

    unsigned prev = m_pGamepad->MenuButtons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->MenuButtons();
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
