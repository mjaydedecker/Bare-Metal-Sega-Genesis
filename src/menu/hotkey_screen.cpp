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
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
#include <circle/timer.h>

#define NUM_ACT  HK_COUNT                 // 6 actions
#define NUM_ROWS (HK_COUNT * 2)           // hold + key per action

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

HotkeyScreen::HotkeyScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                           CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                           SettingsStore *pStore)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore)
{
}

void HotkeyScreen::Render(int selected)
{
    using namespace chrome;
    unsigned conflicts =
        hotkey_conflicts(m_pSettings->hotkeys, m_pSettings->menu_hotkey);

    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "HOTKEYS", "Select + button", theme::VALUE);

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  act   = i / 2;
        bool isKey = (i & 1);
        bool sel   = (i == selected);
        bool bad   = (conflicts >> act) & 1u;

        const HotkeyBinding &b = m_pSettings->hotkeys.b[act];
        char label[28]; int k = 0;
        const char *a = ACT_LABEL[act];
        for (int j = 0; a[j] && k < 18; j++) label[k++] = a[j];
        label[k++] = ' ';
        const char *f = isKey ? "key" : "mod";
        for (int j = 0; f[j]; j++) label[k++] = f[j];
        label[k] = '\0';

        const char *val = phys_label(isKey ? b.trigger : b.hold);
        int y = value_row(m_pCanvas, i, sel, label, val,
                          bad ? theme::SELECTION : theme::VALUE, true);
        if (bad) {
            int W = (int) m_pCanvas->Width();
            m_pCanvas->Text(&g_font_ps2p8, 1, W - PAD - 10, y + 4, "!",
                            theme::SELECTION, theme::BG, true);
        }
    }

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_dpad(m_pCanvas, PAD, fy, "NAVIGATE");
    hx = hint_lr(m_pCanvas, hx, fy, "CHANGE");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
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
