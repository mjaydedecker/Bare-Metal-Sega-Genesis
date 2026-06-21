//
// src/menu/video_mode_screen.cpp
//
// Bare Metal Sega Genesis
// See video_mode_screen.h.
//

#include "video_mode_screen.h"
#include "../input/joypad_map.h"   // GP_LEFT/RIGHT/A/B/START
#include <circle/timer.h>

#define NUM_MODES 4

static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

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

VideoModeScreen::VideoModeScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                                 CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                                 SettingsStore *pStore, Display *pDisplay)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay)
{
}

void VideoModeScreen::Render(VideoMode sel)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2, boxW = cw * 34, boxH = ch * 6;

    m_pCanvas->Clear(0x0000);   // wipe any larger menu drawn before this one
    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "VIDEO MODE", WHITE, BOX);

    char val[16];
    int i = 0; val[i++] = '<'; val[i++] = ' ';
    const char *lbl = mode_label(sel);
    for (int j = 0; lbl[j] && i < 13; j++) val[i++] = lbl[j];
    val[i++] = ' '; val[i++] = '>'; val[i] = '\0';
    m_pCanvas->FillRect(boxX + cw, boxY + ch * 3, boxW - cw * 2, ch, SELBG);
    m_pCanvas->DrawText(boxX + cw * 2, boxY + ch * 3, val, SELFG, SELBG);

    m_pCanvas->DrawText(boxX + cw, boxY + ch * 5,
                        "Start: apply   B: back", WHITE, BOX);
}

boolean VideoModeScreen::Confirm(void)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    unsigned prev = m_pGamepad->MenuButtons();

    for (int sec = 15; sec > 0; sec--)
    {
        m_pCanvas->FillRect(cw * 3, ch * 2, cw * 34, ch * 4, BOX);
        m_pCanvas->DrawText(cw * 4, ch * 3, "Keep this mode?", WHITE, BOX);
        char line[40] = "A: keep    Reverting in 00";
        line[24] = (char) ('0' + (sec / 10));
        line[25] = (char) ('0' + (sec % 10));
        m_pCanvas->DrawText(cw * 4, ch * 4, line, WHITE, BOX);

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
        // Couldn't even allocate the mode; stay on the current one.
        int cw = (int) m_pCanvas->CharW();
        int ch = (int) m_pCanvas->CharH();
        m_pCanvas->FillRect(cw * 3, ch * 2, cw * 34, ch * 3, BOX);
        m_pCanvas->DrawText(cw * 4, ch * 3, "Mode unavailable.", WHITE, BOX);
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
