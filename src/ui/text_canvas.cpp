//
// src/ui/text_canvas.cpp
//
// Bare Metal Sega Genesis
// See text_canvas.h.
//

#include "text_canvas.h"
#include <circle/font.h>

TextCanvas::TextCanvas(Display *pDisplay)
:   m_pDisplay(pDisplay), m_Font(Font12x22)
{
}

unsigned TextCanvas::CharW(void) const { return m_Font.GetCharWidth(); }
unsigned TextCanvas::CharH(void) const { return m_Font.GetCharHeight(); }

unsigned TextCanvas::Cols(void) const
{
    unsigned cw = CharW();
    return cw ? m_pDisplay->Width() / cw : 0;
}

unsigned TextCanvas::Rows(void) const
{
    unsigned ch = CharH();
    return ch ? m_pDisplay->Height() / ch : 0;
}

void TextCanvas::FillRect(int x, int y, int w, int h, u16 color)
{
    u16 *buf = m_pDisplay->Buffer();
    if (buf == 0) return;
    unsigned pitchPx = m_pDisplay->Pitch() / 2;
    int fbw = (int) m_pDisplay->Width();
    int fbh = (int) m_pDisplay->Height();
    for (int yy = y; yy < y + h; yy++)
    {
        if (yy < 0 || yy >= fbh) continue;
        for (int xx = x; xx < x + w; xx++)
        {
            if (xx < 0 || xx >= fbw) continue;
            buf[(unsigned) yy * pitchPx + (unsigned) xx] = color;
        }
    }
}

void TextCanvas::Clear(u16 color)
{
    FillRect(0, 0, (int) m_pDisplay->Width(), (int) m_pDisplay->Height(), color);
}

void TextCanvas::DrawText(int x, int y, const char *s, u16 fg, u16 bg)
{
    u16 *buf = m_pDisplay->Buffer();
    if (buf == 0 || s == 0) return;
    unsigned pitchPx = m_pDisplay->Pitch() / 2;
    int fbw = (int) m_pDisplay->Width();
    int fbh = (int) m_pDisplay->Height();
    unsigned cw = CharW(), ch = CharH();

    int penx = x;
    for (unsigned i = 0; s[i] != '\0'; i++)
    {
        char c = s[i];
        for (unsigned py = 0; py < ch; py++)
        {
            int yy = y + (int) py;
            if (yy < 0 || yy >= fbh) continue;
            for (unsigned px = 0; px < cw; px++)
            {
                int xx = penx + (int) px;
                if (xx < 0 || xx >= fbw) continue;
                boolean on = m_Font.GetPixel(c, px, py);
                buf[(unsigned) yy * pitchPx + (unsigned) xx] = on ? fg : bg;
            }
        }
        penx += (int) cw;
    }
}
