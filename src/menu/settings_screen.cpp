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
#include <string.h>   // strcmp, strncpy for the auto-launch row
#include "controls_screen.h"
#include "video_mode_screen.h"

#define NUM_ROWS 9

// RGB565 colours (match the pause menu palette).
static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay,
                               AudioDriver *pAudio, ControlsScreen *pControls,
                               VideoModeScreen *pVideoMode)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay),
    m_pAudio(pAudio), m_pRomPath(0), m_pControls(pControls),
    m_pVideoMode(pVideoMode)
{
}

void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_region_value    = region_core_value(m_pSettings->region);
    g_variables_dirty = true;                            // ...on next poll/reset
    m_pAudio->SetVolume(m_pSettings->volume);            // live
    m_pAudio->SetMute(m_pSettings->mute);                // live
}

// Format a 0-100 volume as "< NNN >" into out (>= 8 bytes).
static void fmt_volume(char *out, unsigned v)
{
    char rev[4];
    int  n = 0;
    if (v == 0) rev[n++] = '0';
    else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
    int i = 0;
    out[i++] = '<'; out[i++] = ' ';
    while (n) out[i++] = rev[--n];
    out[i++] = ' '; out[i++] = '>'; out[i] = '\0';
}

void SettingsScreen::Render(int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2;
    int boxW = cw * 38, boxH = ch * (NUM_ROWS + 5);

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "SETTINGS", WHITE, BOX);

    char volVal[8];
    fmt_volume(volVal, m_pSettings->volume);
    const char *scaleVal = m_pSettings->scale_mode == ScaleMode::Stretch
                               ? "< Stretch >" : "< Integer >";
    const char *wideVal  = m_pSettings->widescreen ? "< On >" : "< Off >";
    const char *muteVal  = m_pSettings->mute ? "< On >" : "< Off >";
    const char *regionVal =
        m_pSettings->region == Region::NTSC ? "< NTSC >" :
        m_pSettings->region == Region::PAL  ? "< PAL >"  : "< Auto >";
    bool autoOn = m_pRomPath != 0 &&
                  strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
    const char *autoVal = autoOn ? "< On >" : "< Off >";
    const char *hotkeyVal =
        m_pSettings->menu_hotkey == MenuHotkey::StartA ? "< Start+A >" :
        m_pSettings->menu_hotkey == MenuHotkey::StartB ? "< Start+B >" :
        m_pSettings->menu_hotkey == MenuHotkey::LR     ? "< L+R >"     :
                                                         "< Start+Select >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Controls...",
                                     "Video Mode..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, "", "" };

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

    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 3),
                        "Widescreen: on reset.  Region: on reload.",
                        WHITE, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 4),
                        "Auto-launch boots this game.  B: back", WHITE, BOX);
}

void SettingsScreen::Run(void)
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
            switch (selected)
            {
            case 0:   // Video Scale (toggle, either direction)
                m_pSettings->scale_mode =
                    m_pSettings->scale_mode == ScaleMode::Integer
                        ? ScaleMode::Stretch : ScaleMode::Integer;
                break;
            case 1:   // Widescreen (toggle)
                m_pSettings->widescreen = !m_pSettings->widescreen;
                break;
            case 2:   // Volume (+/- 10, clamped 0-100)
            {
                int v = (int) m_pSettings->volume + dir * 10;
                if (v < 0)   v = 0;
                if (v > 100) v = 100;
                m_pSettings->volume = (unsigned) v;
                break;
            }
            case 3:   // Mute (toggle)
                m_pSettings->mute = !m_pSettings->mute;
                break;
            case 4:   // Region (cycle Auto -> NTSC -> PAL)
            {
                int r = (int) m_pSettings->region + dir;
                if (r < 0) r = 2;
                if (r > 2) r = 0;
                m_pSettings->region = (Region) r;
                break;
            }
            case 5:   // Auto-launch this game (toggle)
            {
                bool on = m_pRomPath != 0 &&
                          strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
                if (on)
                {
                    m_pSettings->auto_launch_rom[0] = '\0';
                }
                else if (m_pRomPath != 0)
                {
                    strncpy(m_pSettings->auto_launch_rom, m_pRomPath,
                            sizeof(m_pSettings->auto_launch_rom) - 1);
                    m_pSettings->auto_launch_rom[
                        sizeof(m_pSettings->auto_launch_rom) - 1] = '\0';
                }
                break;
            }
            case 6:   // Menu Hotkey (cycle the 4 presets)
            {
                int h = (int) m_pSettings->menu_hotkey + dir;
                if (h < 0) h = 3;
                if (h > 3) h = 0;
                m_pSettings->menu_hotkey = (MenuHotkey) h;
                break;
            }
            }

            Apply();
            m_pStore->Save(*m_pSettings);
            Render(selected);
        }
        if (pressed & GP_START)
        {
            if (selected == 7)                        // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 8)                   // Video Mode...
            {
                m_pVideoMode->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
        }
        if (pressed & GP_B)
            return;

        CTimer::SimpleMsDelay(16);
    }
}
