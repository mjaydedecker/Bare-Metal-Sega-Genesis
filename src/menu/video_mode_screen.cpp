//
// src/menu/video_mode_screen.cpp
//
// Bare Metal Sega Genesis
// See video_mode_screen.h. Rendered with GlyphCanvas in the v1.0.0 design.
//

#include "video_mode_screen.h"
#include "../input/joypad_map.h"   // GP_LEFT/RIGHT/A/B/START
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
#include <circle/timer.h>

#define NUM_MODES 4

static const char *mode_label(VideoMode m)
{
    switch (m)
    {
    case VideoMode::P1080: return "1080p";
    case VideoMode::P720:  return "720p";
    case VideoMode::P480:  return "480p";
    default:               return "Native";
    }
}

VideoModeScreen::VideoModeScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                                 CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                                 SettingsStore *pStore, Display *pDisplay)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay)
{
}

void VideoModeScreen::Render(VideoMode sel)
{
    using namespace chrome;
    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "VIDEO MODE", "HDMI OUTPUT", theme::VALUE);

    const VideoMode modes[NUM_MODES] = { VideoMode::Native, VideoMode::P1080,
                                         VideoMode::P720, VideoMode::P480 };
    int x = PAD;
    int y = LIST_TOP;
    for (int i = 0; i < NUM_MODES; i++)
    {
        const char *lbl = mode_label(modes[i]);
        int tw = m_pCanvas->TextWidth(&g_font_vt323_22, 1, lbl);
        int chipW = tw + 28;
        bool on = (modes[i] == sel);
        if (on) m_pCanvas->FillRect(x, y, chipW, 30, theme::SELECTION);
        else    { m_pCanvas->FillRect(x, y, chipW, 1, theme::TEXT_DIM);
                  m_pCanvas->FillRect(x, y + 29, chipW, 1, theme::TEXT_DIM); }
        m_pCanvas->Text(&g_font_vt323_22, 1, x + 14, y + 4, lbl,
                        on ? theme::WHITE : theme::TEXT_MUTED, theme::BG, true);
        x += chipW + 12;
    }

    m_pCanvas->Text(&g_font_vt323_22, 1, PAD, y + 50,
                    "Only a confirmed mode is saved; an unsupported signal auto-reverts.",
                    theme::TEXT_DIM, theme::BG, true);

    footer_divider(m_pCanvas);
    int H = (int) m_pCanvas->Height();
    int fy = H - FOOT_H + 4;
    int hx = hint_lr(m_pCanvas, PAD, fy, "SELECT");
    hx = hint_button(m_pCanvas, hx, fy, 'A', theme::SELECTION, "APPLY");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
}

boolean VideoModeScreen::Confirm(void)
{
    using namespace chrome;
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();
    unsigned prev = m_pGamepad->MenuButtons();

    for (int sec = 15; sec > 0; sec--)
    {
        int bw = 560, bh = 200;
        int bx = W / 2 - bw / 2, by = H / 2 - bh / 2;
        m_pCanvas->FillRect(bx, by, bw, bh, theme::BG);
        m_pCanvas->FillRect(bx, by, bw, 3, theme::ADJUST);
        m_pCanvas->Text(&g_font_ps2p8, 2, bx + 24, by + 22, "KEEP THIS MODE?",
                        theme::ADJUST, theme::BG, true);

        char line[40] = "Reverting in 00";
        line[13] = (char) ('0' + (sec / 10));
        line[14] = (char) ('0' + (sec % 10));
        m_pCanvas->Text(&g_font_vt323_22, 1, bx + 24, by + 70, line,
                        theme::WHITE, theme::BG, true);

        int hx = hint_button(m_pCanvas, bx + 24, by + 110, 'A', theme::ACTIVE, "KEEP");
        hint_button(m_pCanvas, hx, by + 110, 'B', theme::TEXT_DIM, "REVERT");

        // progress bar (drains with sec)
        int barW = bw - 48;
        m_pCanvas->FillRect(bx + 24, by + 150, barW, 8, theme::TEXT_DIM);
        m_pCanvas->FillRect(bx + 24, by + 150, barW * sec / 15, 8, theme::ADJUST);

        m_pCanvas->Scanlines(0, 0, W, H, 60);

        for (int t = 0; t < 62; t++)   // ~1 s of 16 ms polls
        {
            m_pUSBHCI->UpdatePlugAndPlay();
            m_pGamepad->Poll();
            unsigned now     = m_pGamepad->MenuButtons();
            unsigned pressed = now & ~prev;
            prev = now;
            if (pressed & GP_A) return TRUE;
            if (pressed & GP_B) return FALSE;
            CTimer::SimpleMsDelay(16);
        }
    }
    return FALSE;
}

void VideoModeScreen::Apply(VideoMode want)
{
    VideoMode prevMode = m_pSettings->video_mode;
    unsigned pw, ph; video_mode_dims(prevMode, pw, ph);
    unsigned w, h;   video_mode_dims(want, w, h);

    if (!m_pDisplay->SetMode(w, h))
    {
        int W = (int) m_pCanvas->Width();
        int H = (int) m_pCanvas->Height();
        int bw = 460, bh = 70, bx = W/2-bw/2, by = H/2-bh/2;
        m_pCanvas->FillRect(bx, by, bw, bh, theme::BG);
        m_pCanvas->FillRect(bx, by, bw, 3, theme::SELECTION);
        m_pCanvas->Text(&g_font_vt323_22, 1, bx + 24, by + 24,
                        "Mode unavailable.", theme::TEXT, theme::BG, true);
        CTimer::SimpleMsDelay(1200);
        return;
    }

    if (Confirm())
    {
        m_pSettings->video_mode = want;
        m_pStore->Save(*m_pSettings);
    }
    else
    {
        m_pDisplay->SetMode(pw, ph);   // revert to the known-good mode
    }
}

void VideoModeScreen::Run(void)
{
    VideoMode sel = m_pSettings->video_mode;
    Render(sel);

    unsigned prev = m_pGamepad->MenuButtons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->MenuButtons();
        unsigned pressed = now & ~prev;
        prev = now;

        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            int v = ((int) sel + dir + NUM_MODES) % NUM_MODES;
            sel = (VideoMode) v;
            Render(sel);
        }

        if (pressed & GP_START)
        {
            Apply(sel);
            sel = m_pSettings->video_mode;            // reflect what stuck
            prev = m_pGamepad->MenuButtons();         // resync
            Render(sel);
        }

        if (pressed & GP_B)
        {
            return;
        }

        CTimer::SimpleMsDelay(16);
    }
}
