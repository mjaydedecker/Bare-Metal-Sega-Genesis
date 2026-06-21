//
// src/menu/hotkey_screen.cpp
//
// Bare Metal Sega Genesis
// See hotkey_screen.h.
//

#include "hotkey_screen.h"
#include "../input/joypad_map.h"          // GP_* for nav
#include "../input/hotkey.h"              // hotkey_hold_mask, hotkey_conflicts
#include "../libretro/callbacks.h"        // g_hotkey_hold_mask
#include <circle/timer.h>

#define NUM_ACT  HK_COUNT                 // 6 actions
#define NUM_ROWS (HK_COUNT * 2)           // hold + key per action

static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 RED   = 0xF800;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

static const char *const ACT_LABEL[NUM_ACT] =
    { "Quick-save", "Quick-load", "Vol +", "Vol -", "HUD", "Mute" };

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

HotkeyScreen::HotkeyScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                           CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                           SettingsStore *pStore)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore)
{
}

void HotkeyScreen::Render(int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2;
    int boxW = cw * 36, boxH = ch * (NUM_ROWS + 4);

    unsigned conflicts =
        hotkey_conflicts(m_pSettings->hotkeys, m_pSettings->menu_hotkey);

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "HOTKEYS", WHITE, BOX);

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  act   = i / 2;
        bool isKey = (i & 1);
        int  ty    = boxY + ch * (i + 3);
        bool sel   = (i == selected);
        bool bad   = (conflicts >> act) & 1u;
        u16  fg    = sel ? SELFG : (bad ? RED : WHITE);
        u16  bg    = sel ? SELBG : BOX;
        if (sel) m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        m_pCanvas->DrawText(boxX + cw, ty, sel ? ">" : " ", fg, bg);

        const HotkeyBinding &b = m_pSettings->hotkeys.b[act];
        char label[24];
        int  k = 0;
        const char *a = ACT_LABEL[act];
        for (int j = 0; a[j] && k < 16; j++) label[k++] = a[j];
        label[k++] = ' ';
        const char *f = isKey ? "key" : "mod";
        for (int j = 0; f[j] && k < 22; j++) label[k++] = f[j];
        label[k] = '\0';

        char val[12];
        val[0] = '<'; val[1] = ' ';
        const char *pl = phys_label(isKey ? b.trigger : b.hold);
        k = 2;
        for (int j = 0; pl[j] && k < 9; j++) val[k++] = pl[j];
        val[k++] = ' '; val[k++] = '>'; val[k] = '\0';

        m_pCanvas->DrawText(boxX + cw * 3,  ty, label, fg, bg);
        m_pCanvas->DrawText(boxX + cw * 18, ty, val,   fg, bg);
        if (bad) m_pCanvas->DrawText(boxX + cw * 25, ty, "!", fg, bg);
    }

    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 3),
                        "Left/Right: change   B: back", WHITE, BOX);
}

void HotkeyScreen::Run(void)
{
    int selected = 0;
    Render(selected);

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
            Render(selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = (selected + 1) % NUM_ROWS;
            Render(selected);
        }

        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            int  act   = selected / 2;
            bool isKey = (selected & 1);
            HotkeyBinding &b = m_pSettings->hotkeys.b[act];

            if (isKey)
            {
                // Cycle trigger over all 8 PadButtons, skipping the hold value.
                int v = (int) b.trigger;
                do { v = (v + dir + 8) % 8; } while (v == (int) b.hold);
                b.trigger = (PadButton) v;
            }
            else
            {
                // Cycle hold over the safe set L,R,Start,Select (values 4..7).
                int v = (int) b.hold;
                v = ((v - 4 + dir + 4) % 4) + 4;
                b.hold = (PadButton) v;
                if (b.hold == b.trigger)                 // keep hold != trigger
                    b.trigger = (PadButton) (((int) b.trigger + 1) % 8);
            }

            m_pStore->Save(*m_pSettings);
            g_hotkey_hold_mask = hotkey_hold_mask(m_pSettings->hotkeys);
            Render(selected);
        }

        if (pressed & GP_B)
        {
            return;
        }

        CTimer::SimpleMsDelay(16);
    }
}
