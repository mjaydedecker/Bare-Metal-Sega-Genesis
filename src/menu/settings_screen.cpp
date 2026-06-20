//
// src/menu/settings_screen.cpp
//
// Bare Metal Sega Genesis
// See settings_screen.h.
//

#include "settings_screen.h"
#include "../input/joypad_map.h"          // GP_UP, GP_DOWN, GP_LEFT, GP_RIGHT, GP_B
#include "../libretro/environment.h"      // g_widescreen, g_variables_dirty
#include <circle/timer.h>

#define NUM_ROWS 2

// RGB565 colours (match the pause menu palette).
static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay)
{
}

void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_variables_dirty = true;                            // ...on next poll/reset
}

void SettingsScreen::Render(int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2;
    int boxW = cw * 38, boxH = ch * (NUM_ROWS + 5);

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "SETTINGS", WHITE, BOX);

    const char *scaleVal = m_pSettings->scale_mode == ScaleMode::Stretch
                               ? "< Stretch >" : "< Integer >";
    const char *wideVal  = m_pSettings->widescreen ? "< On >" : "< Off >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal };

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  ty  = boxY + ch * (i + 3);
        bool sel = (i == selected);
        u16  fg  = sel ? SELFG : WHITE;
        u16  bg  = sel ? SELBG : BOX;
        if (sel) m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        m_pCanvas->DrawText(boxX + cw,      ty, sel ? ">" : " ", fg, bg);
        m_pCanvas->DrawText(boxX + cw * 3,  ty, labels[i],       fg, bg);
        m_pCanvas->DrawText(boxX + cw * 17, ty, values[i],       fg, bg);
    }

    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 4),
                        "Widescreen applies on reset.  B: back", WHITE, BOX);
}

void SettingsScreen::Run(void)
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
            selected = (selected + NUM_ROWS - 1) % NUM_ROWS;
            Render(selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = (selected + 1) % NUM_ROWS;
            Render(selected);
        }
        if (pressed & (GP_LEFT | GP_RIGHT))
        {
            if (selected == 0)
                m_pSettings->scale_mode =
                    m_pSettings->scale_mode == ScaleMode::Integer
                        ? ScaleMode::Stretch : ScaleMode::Integer;
            else
                m_pSettings->widescreen = !m_pSettings->widescreen;

            Apply();
            m_pStore->Save(*m_pSettings);
            Render(selected);
        }
        if (pressed & GP_B)
            return;

        CTimer::SimpleMsDelay(16);
    }
}
