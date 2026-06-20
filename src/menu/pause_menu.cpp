//
// src/menu/pause_menu.cpp
//
// Bare Metal Sega Genesis
// See pause_menu.h.
//

#include "pause_menu.h"
#include "menu_state.h"             // menu_next_enabled
#include "../input/joypad_map.h"    // GP_UP, GP_DOWN, GP_START
#include <circle/timer.h>

#define NUM_ENTRIES 6

static const char *const LABELS[NUM_ENTRIES] =
{
    "Resume", "Save State", "Load State", "Reset Game", "Settings",
    "Return to ROM Browser"
};
static const bool ENABLED[NUM_ENTRIES] = { true, false, false, true, false, true };
static const MenuAction ACTIONS[NUM_ENTRIES] =
{
    MenuAction::Resume, MenuAction::Resume, MenuAction::Resume,
    MenuAction::Reset,  MenuAction::Resume, MenuAction::ReturnToBrowser
};   // disabled entries' actions are never reached

// RGB565 colours.
static const u16 BOX   = 0x0008;   // near-black box
static const u16 WHITE = 0xFFFF;
static const u16 GREY  = 0x8410;
static const u16 SELFG = 0x0000;   // black text on the selection bar
static const u16 SELBG = 0x07FF;   // cyan selection bar

PauseMenu::PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI)
{
}

void PauseMenu::Render(int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3;
    int boxY = ch * 2;
    int boxW = cw * 26;
    int boxH = ch * (NUM_ENTRIES + 3);

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "PAUSED", WHITE, BOX);

    for (int i = 0; i < NUM_ENTRIES; i++)
    {
        int ty = boxY + ch * (i + 3);
        bool sel = (i == selected);
        u16 fg = sel ? SELFG : (ENABLED[i] ? WHITE : GREY);
        u16 bg = sel ? SELBG : BOX;
        if (sel)
        {
            m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        }
        m_pCanvas->DrawText(boxX + cw,     ty, sel ? ">" : " ", fg, bg);
        m_pCanvas->DrawText(boxX + cw * 3, ty, LABELS[i],       fg, bg);
    }
}

MenuAction PauseMenu::Run(void)
{
    int selected = 0;   // Resume (enabled)
    Render(selected);

    // Require the hotkey buttons to be released before Start counts as confirm,
    // so opening the menu (Start+Select) does not instantly select Resume.
    unsigned prev = m_pGamepad->Buttons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)
        {
            selected = menu_next_enabled(ENABLED, NUM_ENTRIES, selected, -1);
            Render(selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = menu_next_enabled(ENABLED, NUM_ENTRIES, selected, +1);
            Render(selected);
        }
        if (pressed & GP_START)
        {
            return ACTIONS[selected];
        }

        CTimer::SimpleMsDelay(16);
    }
}
