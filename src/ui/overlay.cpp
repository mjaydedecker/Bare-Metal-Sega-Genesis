//
// src/ui/overlay.cpp
//
// Bare Metal Sega Genesis
// See overlay.h.
//

#include "overlay.h"

Overlay::Overlay(TextCanvas *pCanvas)
:   m_pCanvas(pCanvas), m_Enabled(false), m_ToastFrames(0)
{
    m_Toast[0] = '\0';
}

void Overlay::Draw(const HudStats &s)
{
    if (!m_Enabled) return;

    char     lines[HUD_LINES][HUD_COLS + 1];
    unsigned n  = hud_format(s, lines, HUD_LINES);
    int      cw = (int) m_pCanvas->CharW();
    int      ch = (int) m_pCanvas->CharH();

    // Fixed opaque box at top-left so prior text is always overwritten (no
    // ghosting across the two vsync pages).
    int x = cw;
    int y = ch;
    m_pCanvas->FillRect(x, y, cw * (HUD_COLS + 1), ch * (HUD_LINES + 1), 0x0000);
    for (unsigned i = 0; i < n; i++)
    {
        m_pCanvas->DrawText(x + cw / 2, y + ch / 2 + ch * (int) i,
                            lines[i], 0xFFFF, 0x0000);   // white on black
    }
}

void Overlay::ShowToast(const char *msg)
{
    unsigned i = 0;
    if (msg != 0)
        for (; msg[i] != '\0' && i < TOAST_MAX; i++) m_Toast[i] = msg[i];
    m_Toast[i] = '\0';
    m_ToastFrames = TOAST_FRAMES;
}

void Overlay::DrawToast(void)
{
    if (m_ToastFrames == 0) return;
    m_ToastFrames--;

    int cw   = (int) m_pCanvas->CharW();
    int ch   = (int) m_pCanvas->CharH();
    int cols = (int) m_pCanvas->Cols();
    int rows = (int) m_pCanvas->Rows();

    int len = 0; while (m_Toast[len] != '\0') len++;

    // Opaque box, centered horizontally, near the bottom (HUD is top-left).
    int boxW = (len + 2) * cw;
    int boxH = 2 * ch;
    int x = (cols * cw - boxW) / 2;
    if (x < 0) x = 0;
    int y = (rows - 3) * ch;
    m_pCanvas->FillRect(x, y, boxW, boxH, 0x0000);
    m_pCanvas->DrawText(x + cw, y + ch / 2, m_Toast, 0xFFFF, 0x0000);
}
