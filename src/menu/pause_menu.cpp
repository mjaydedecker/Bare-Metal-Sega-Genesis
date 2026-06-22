//
// src/menu/pause_menu.cpp
//
// Bare Metal Sega Genesis
// See pause_menu.h. Rendered with GlyphCanvas in the v1.0.0 design: a glowing
// panel over the dimmed (not wiped) live game frame.
//

#include "pause_menu.h"
#include "settings_screen.h"
#include "menu_state.h"             // menu_next_enabled
#include "../input/joypad_map.h"    // GP_UP, GP_DOWN, GP_START, GP_B
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
#include <circle/timer.h>

#define NUM_ENTRIES 6
#define NUM_SLOTS   4

static const char *const LABELS[NUM_ENTRIES] =
{
    "Resume", "Save State", "Load State", "Reset Game", "Settings",
    "Return to ROM Browser"
};
static const bool ENABLED[NUM_ENTRIES] = { true, true, true, true, true, true };

PauseMenu::PauseMenu(GlyphCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                     SaveState *pSaveState, SettingsScreen *pSettingsScreen)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSaveState(pSaveState), m_pSettingsScreen(pSettingsScreen)
{
}

void PauseMenu::Render(int selected)
{
    using namespace chrome;
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();

    // Dim the live game frame behind the panel (don't wipe to black).
    m_pCanvas->BlendRect(0, 0, W, H, theme::BG, 200);

    int panelW = 460;
    int panelH = NUM_ENTRIES * ROW_H + 120;
    int px = W / 2 - panelW / 2;
    int py = H / 2 - panelH / 2;
    m_pCanvas->FillRect(px, py, panelW, panelH, theme::BG);
    m_pCanvas->FillRect(px, py, panelW, 3, theme::SELECTION);          // top accent
    m_pCanvas->FillRect(px, py + panelH - 3, panelW, 3, theme::SELECTION);

    const char *title = "|| PAUSED";
    int tw = m_pCanvas->TextWidth(&g_font_ps2p8, 2, title);
    m_pCanvas->Text(&g_font_ps2p8, 2, W / 2 - tw / 2, py + 20, title,
                    theme::WHITE, theme::BG, true);

    for (int i = 0; i < NUM_ENTRIES; i++)
    {
        int ty = py + 64 + i * ROW_H;
        bool sel = (i == selected);
        if (sel) m_pCanvas->FillRect(px + 16, ty - 2, panelW - 32, ROW_H,
                                     theme::SELECTION);
        if (sel) m_pCanvas->IconPlay(px + 22, ty + 4, 16, theme::WHITE);
        m_pCanvas->Text(&g_font_vt323_22, 1, px + 48, ty, LABELS[i],
                        sel ? theme::WHITE : theme::TEXT, theme::BG, true);
    }

    int fy = py + panelH - 28;
    int hx = hint_dpad(m_pCanvas, px + 20, fy - 4, "MOVE");
    hint_start(m_pCanvas, hx, fy - 4, "SELECT");
    m_pCanvas->Scanlines(0, 0, W, H, 60);
}

void PauseMenu::Message(const char *text)
{
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();
    int bw = 460, bh = 70;
    int bx = W / 2 - bw / 2, by = H / 2 - bh / 2;
    m_pCanvas->FillRect(bx, by, bw, bh, theme::BG);
    m_pCanvas->FillRect(bx, by, bw, 3, theme::SELECTION);
    m_pCanvas->Text(&g_font_vt323_22, 1, bx + 24, by + 24, text,
                    theme::TEXT, theme::BG, true);
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

    bool     redraw = true;
    unsigned prev   = m_pGamepad->MenuButtons();
    for (;;)
    {
        if (redraw)
        {
            int W = (int) m_pCanvas->Width();
            int H = (int) m_pCanvas->Height();
            m_pCanvas->BlendRect(0, 0, W, H, theme::BG, 200);

            int panelW = 460, panelH = NUM_SLOTS * 44 + 120;
            int px = W / 2 - panelW / 2, py = H / 2 - panelH / 2;
            m_pCanvas->FillRect(px, py, panelW, panelH, theme::BG);
            m_pCanvas->FillRect(px, py, panelW, 3, theme::SELECTION);

            const char *ttl = forLoad ? "LOAD STATE" : "SAVE STATE";
            int tw = m_pCanvas->TextWidth(&g_font_ps2p8, 2, ttl);
            m_pCanvas->Text(&g_font_ps2p8, 2, W / 2 - tw / 2, py + 18, ttl,
                            theme::WHITE, theme::BG, true);

            for (int i = 0; i < NUM_SLOTS; i++)
            {
                int ty = py + 64 + i * 44;
                bool cur = (i == sel) && selable[i];
                if (cur) m_pCanvas->FillRect(px + 16, ty - 2, panelW - 32, 38,
                                             theme::SELECTION);
                if (cur) m_pCanvas->IconPlay(px + 22, ty + 6, 16, theme::WHITE);

                char label[8] = { 'S','l','o','t',' ', (char)('1'+i), '\0' };
                u16 lc = cur ? theme::WHITE : (selable[i] ? theme::TEXT : theme::TEXT_DIM);
                m_pCanvas->Text(&g_font_vt323_22, 1, px + 48, ty, label,
                                lc, theme::BG, true);

                // USED / EMPTY badge, right-aligned.
                const char *badge = occupied[i] ? "USED" : "EMPTY";
                u16 bf = occupied[i] ? theme::ACTIVE : theme::TEXT_DIM;
                int bw = m_pCanvas->TextWidth(&g_font_ps2p8, 1, badge);
                if (occupied[i])
                    m_pCanvas->FillRect(px + panelW - 40 - bw - 12, ty + 2,
                                        bw + 16, 22, theme::ACTIVE);
                m_pCanvas->Text(&g_font_ps2p8, 1, px + panelW - 40 - bw - 4, ty + 6,
                                badge, occupied[i] ? theme::BG : bf, theme::BG, true);
            }

            int fy = py + panelH - 28;
            int hx = chrome::hint_start(m_pCanvas, px + 20, fy - 4,
                                        forLoad ? "LOAD" : "SAVE");
            chrome::hint_button(m_pCanvas, hx, fy - 4, 'B', theme::TEXT_DIM, "CANCEL");
            m_pCanvas->Scanlines(0, 0, W, H, 60);
            redraw = false;
        }

        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->MenuButtons();
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
                prev = m_pGamepad->MenuButtons();
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
                prev = m_pGamepad->MenuButtons();
                break;
            }

            case 3:                       // Reset Game
                return MenuAction::Reset;

            case 4:                       // Settings
                m_pSettingsScreen->Run();
                Render(selected);
                prev = m_pGamepad->MenuButtons();
                break;

            case 5:                       // Return to ROM Browser
                return MenuAction::ReturnToBrowser;

            default:
                break;
            }
        }

        CTimer::SimpleMsDelay(16);
    }
}
