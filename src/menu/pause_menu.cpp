//
// src/menu/pause_menu.cpp
//
// Bare Metal Sega Genesis
// See pause_menu.h.
//

#include "pause_menu.h"
#include "menu_state.h"             // menu_next_enabled
#include "../input/joypad_map.h"    // GP_UP, GP_DOWN, GP_START, GP_B
#include <circle/timer.h>

#define NUM_ENTRIES 6
#define NUM_SLOTS   4

static const char *const LABELS[NUM_ENTRIES] =
{
    "Resume", "Save State", "Load State", "Reset Game", "Settings",
    "Return to ROM Browser"
};
static const bool ENABLED[NUM_ENTRIES] = { true, true, true, true, false, true };

// RGB565 colours.
static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 GREY  = 0x8410;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

PauseMenu::PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                     SaveState *pSaveState)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSaveState(pSaveState)
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

void PauseMenu::Message(const char *text)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    m_pCanvas->FillRect(cw * 3, ch * 2, cw * 26, ch * 3, BOX);
    m_pCanvas->DrawText(cw * 4, ch * 3, text, WHITE, BOX);
    CTimer::SimpleMsDelay(1200);
}

int PauseMenu::PickSlot(bool forLoad)
{
    bool occupied[NUM_SLOTS];
    bool selable[NUM_SLOTS];
    for (int i = 0; i < NUM_SLOTS; i++)
    {
        occupied[i] = m_pSaveState->Occupied(i + 1);
        selable[i]  = forLoad ? occupied[i] : true;
    }

    int sel = 0;
    if (forLoad)                          // start on the first occupied slot
    {
        sel = 0;
        for (int i = 0; i < NUM_SLOTS; i++) if (occupied[i]) { sel = i; break; }
    }

    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();

    bool     redraw = true;
    unsigned prev   = m_pGamepad->Buttons();
    for (;;)
    {
        if (redraw)
        {
            m_pCanvas->FillRect(cw * 3, ch * 2, cw * 26, ch * (NUM_SLOTS + 5), BOX);
            m_pCanvas->DrawText(cw * 4, ch * 3, forLoad ? "Load State" : "Save State",
                                WHITE, BOX);
            for (int i = 0; i < NUM_SLOTS; i++)
            {
                int ty = ch * (i + 5);
                bool cur = (i == sel) && selable[i];
                u16 fg = cur ? SELFG : (selable[i] ? WHITE : GREY);
                u16 bg = cur ? SELBG : BOX;
                if (cur) m_pCanvas->FillRect(cw * 4, ty, cw * 24, ch, SELBG);
                char digit[2] = { (char) ('1' + i), '\0' };
                m_pCanvas->DrawText(cw * 5,  ty, "Slot", fg, bg);
                m_pCanvas->DrawText(cw * 10, ty, digit,  fg, bg);
                m_pCanvas->DrawText(cw * 13, ty, occupied[i] ? "Used" : "Empty", fg, bg);
            }
            m_pCanvas->DrawText(cw * 4, ch * (NUM_SLOTS + 6),
                                "Start: select   B: cancel", WHITE, BOX);
            redraw = false;
        }

        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)
        {
            int n = menu_next_enabled(selable, NUM_SLOTS, sel, -1);
            if (n != sel) { sel = n; redraw = true; }
        }
        if (pressed & GP_DOWN)
        {
            int n = menu_next_enabled(selable, NUM_SLOTS, sel, +1);
            if (n != sel) { sel = n; redraw = true; }
        }
        if (pressed & GP_B)
        {
            return 0;                     // cancel
        }
        if (pressed & GP_START)
        {
            if (selable[sel]) return sel + 1;   // 1..4
        }

        CTimer::SimpleMsDelay(16);
    }
}

MenuAction PauseMenu::Run(void)
{
    int selected = 0;
    Render(selected);

    unsigned prev = m_pGamepad->Buttons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->Buttons();
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
            switch (selected)
            {
            case 0:                       // Resume
                return MenuAction::Resume;

            case 1: {                     // Save State
                int slot = PickSlot(false);
                if (slot != 0)
                {
                    bool ok = m_pSaveState->Save(slot);
                    char msg[24] = "Saved to slot 0";
                    msg[14] = (char) ('0' + slot);
                    Message(ok ? msg : "Save failed.");
                }
                Render(selected);
                prev = m_pGamepad->Buttons();
                break;
            }

            case 2: {                     // Load State
                int slot = PickSlot(true);
                if (slot != 0)
                {
                    if (m_pSaveState->Load(slot)) return MenuAction::Resume;
                    Message("Load failed.");
                }
                Render(selected);
                prev = m_pGamepad->Buttons();
                break;
            }

            case 3:                       // Reset Game
                return MenuAction::Reset;

            case 5:                       // Return to ROM Browser
                return MenuAction::ReturnToBrowser;

            default:
                break;
            }
        }

        CTimer::SimpleMsDelay(16);
    }
}
