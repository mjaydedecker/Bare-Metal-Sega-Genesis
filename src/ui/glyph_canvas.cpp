//
// src/ui/glyph_canvas.cpp
//
// Bare Metal Sega Genesis
// See glyph_canvas.h. Every public call maps logical -> physical by the
// current UI scale before reaching the pure primitives (which clip physically).
//
#include "glyph_canvas.h"
#include "glyph_draw.h"
#include "icons.h"
#include "ui_scale.h"

GlyphCanvas::GlyphCanvas(Display *pDisplay) : m_pDisplay(pDisplay) {}

int GlyphCanvas::Scale(void) const {
    return (int) ui_scale(m_pDisplay->Width(), m_pDisplay->Height());
}

unsigned GlyphCanvas::Width(void)  const { return m_pDisplay->Width()  / (unsigned) Scale(); }
unsigned GlyphCanvas::Height(void) const { return m_pDisplay->Height() / (unsigned) Scale(); }

void GlyphCanvas::FillRect(int x, int y, int w, int h, u16 color) {
    int s = Scale();
    gd_fill_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x * s, y * s, w * s, h * s, color);
}

void GlyphCanvas::Clear(u16 color) {
    // Physical size: logical Width()*scale can fall up to scale-1 px short.
    gd_fill_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 0, 0, (int) m_pDisplay->Width(), (int) m_pDisplay->Height(), color);
}

void GlyphCanvas::BlendRect(int x, int y, int w, int h, u16 color, u8 alpha) {
    int s = Scale();
    gd_blend_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                  (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                  x * s, y * s, w * s, h * s, color, alpha);
}

void GlyphCanvas::Scanlines(int x, int y, int w, int h, u8 strength) {
    int s = Scale();
    gd_scanlines(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x * s, y * s, w * s, h * s, strength, s);
}

void GlyphCanvas::StippleRect(int x, int y, int w, int h, u16 color) {
    int s = Scale();
    gd_stipple_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                    (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                    x * s, y * s, w * s, h * s, color, s);
}

int GlyphCanvas::Text(const Font *f, int scale, int x, int y, const char *s,
                      u16 fg, u16 bg, bool transparent) {
    int k = Scale();
    int penx = gd_draw_text(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                            (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                            f, scale * k, x * k, y * k, s, fg, bg, transparent);
    return penx / k;   // exact: every advance is a multiple of k
}

int GlyphCanvas::TextWidth(const Font *f, int scale, const char *s) const {
    return gd_text_width(f, scale, s);
}

void GlyphCanvas::IconPlay(int x, int y, int size, u16 color) {
    int s = Scale();
    icon_play(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
              (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
              x * s, y * s, size * s, color);
}

void GlyphCanvas::IconTri(int x, int y, int size, int dir, u16 color) {
    int s = Scale();
    icon_tri(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
             (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
             x * s, y * s, size * s, dir, color);
}

void GlyphCanvas::IconCross(int x, int y, int size, u16 color) {
    int s = Scale();
    icon_cross(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
               (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
               x * s, y * s, size * s, color);
}

void GlyphCanvas::IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                             const Font *f) {
    int s = Scale();
    icon_button(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                x * s, y * s, d * s, letter, fill, fg, f, s);
}
