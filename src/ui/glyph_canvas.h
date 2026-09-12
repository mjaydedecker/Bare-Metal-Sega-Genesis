//
// src/ui/glyph_canvas.h
//
// Bare Metal Sega Genesis
// Circle binding for the pure glyph_draw/icons primitives: draws into the
// Display's RGB565 front page. Successor to TextCanvas for redesigned screens.
//
// Coordinates are LOGICAL: the canvas divides the physical framebuffer by an
// integer UI scale (ui_scale, recomputed on every call so live video-mode
// changes apply at once) and multiplies every position, size and font scale on
// the way out. Screens lay out in Width() x Height() (always >= 640x480) and
// automatically grow at 1080p and above.
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

    unsigned Width(void) const;    // logical
    unsigned Height(void) const;   // logical

    void Clear(u16 color);         // whole physical framebuffer
    void FillRect(int x, int y, int w, int h, u16 color);
    void BlendRect(int x, int y, int w, int h, u16 color, u8 alpha);
    void Scanlines(int x, int y, int w, int h, u8 strength);
    void StippleRect(int x, int y, int w, int h, u16 color);

    // Returns the logical pen x after the last glyph.
    int  Text(const Font *f, int scale, int x, int y, const char *s,
              u16 fg, u16 bg, bool transparent);
    int  TextWidth(const Font *f, int scale, const char *s) const;   // logical

    void IconPlay (int x, int y, int size, u16 color);
    void IconTri  (int x, int y, int size, int dir, u16 color);
    void IconCross(int x, int y, int size, u16 color);
    void IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                    const Font *f);

private:
    int Scale(void) const;         // current integer UI scale (>= 1)

    Display *m_pDisplay;
};

#endif
