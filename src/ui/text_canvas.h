//
// src/ui/text_canvas.h
//
// Bare Metal Sega Genesis
// Draws text and filled rectangles into the Display's RGB565 framebuffer using
// Circle's CCharGenerator font. The single primitive for on-screen UI.
//

#ifndef _ui_text_canvas_h
#define _ui_text_canvas_h

#include <circle/chargenerator.h>
#include <circle/types.h>
#include "../video/display.h"

class TextCanvas
{
public:
    TextCanvas(Display *pDisplay);

    unsigned CharW(void) const;   // glyph width in pixels
    unsigned CharH(void) const;   // glyph height in pixels
    unsigned Cols(void) const;    // Width()  / CharW()
    unsigned Rows(void) const;    // Height() / CharH()

    void Clear(u16 color);
    void FillRect(int x, int y, int w, int h, u16 color);          // pixels, clipped
    void DrawText(int x, int y, const char *s, u16 fg, u16 bg);    // glyph px = fg, cell = bg

private:
    Display       *m_pDisplay;
    CCharGenerator m_Font;
};

#endif
