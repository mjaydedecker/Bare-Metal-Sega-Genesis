//
// src/ui/glyph_canvas.h
//
// Bare Metal Sega Genesis
// Circle binding for the pure glyph_draw/icons primitives: draws into the
// Display's RGB565 front page. Successor to TextCanvas for redesigned screens.
//
#ifndef _ui_glyph_canvas_h
#define _ui_glyph_canvas_h
#include <circle/types.h>
#include "font.h"
#include "../video/display.h"

class GlyphCanvas
{
public:
    GlyphCanvas(Display *pDisplay);

    unsigned Width(void) const;
    unsigned Height(void) const;

    void Clear(u16 color);
    void FillRect(int x, int y, int w, int h, u16 color);
    void BlendRect(int x, int y, int w, int h, u16 color, u8 alpha);
    void Scanlines(int x, int y, int w, int h, u8 strength);
    void StippleRect(int x, int y, int w, int h, u16 color);

    int  Text(const Font *f, int scale, int x, int y, const char *s,
              u16 fg, u16 bg, bool transparent);
    int  TextWidth(const Font *f, int scale, const char *s) const;

    void IconPlay (int x, int y, int size, u16 color);
    void IconTri  (int x, int y, int size, int dir, u16 color);
    void IconCross(int x, int y, int size, u16 color);
    void IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                    const Font *f);

private:
    Display *m_pDisplay;
};

#endif
