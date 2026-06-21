//
// src/ui/overlay.cpp
//
// Bare Metal Sega Genesis
// See overlay.h.
//

#include "overlay.h"

Overlay::Overlay(TextCanvas *pCanvas)
:   m_pCanvas(pCanvas), m_Enabled(false)
{
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
