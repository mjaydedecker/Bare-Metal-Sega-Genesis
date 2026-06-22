//
// src/ui/glyph_canvas.cpp
//
// Bare Metal Sega Genesis
// See glyph_canvas.h.
//
#include "glyph_canvas.h"
#include "glyph_draw.h"
#include "icons.h"

GlyphCanvas::GlyphCanvas(Display *pDisplay) : m_pDisplay(pDisplay) {}

unsigned GlyphCanvas::Width(void)  const { return m_pDisplay->Width(); }
unsigned GlyphCanvas::Height(void) const { return m_pDisplay->Height(); }

void GlyphCanvas::FillRect(int x, int y, int w, int h, u16 color) {
    gd_fill_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x, y, w, h, color);
}

void GlyphCanvas::Clear(u16 color) {
    FillRect(0, 0, (int) m_pDisplay->Width(), (int) m_pDisplay->Height(), color);
}

void GlyphCanvas::BlendRect(int x, int y, int w, int h, u16 color, u8 alpha) {
    gd_blend_rect(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                  (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                  x, y, w, h, color, alpha);
}

void GlyphCanvas::Scanlines(int x, int y, int w, int h, u8 strength) {
    gd_scanlines(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                 (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                 x, y, w, h, strength);
}

int GlyphCanvas::Text(const Font *f, int scale, int x, int y, const char *s,
                      u16 fg, u16 bg, bool transparent) {
    return gd_draw_text(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                        (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                        f, scale, x, y, s, fg, bg, transparent);
}

int GlyphCanvas::TextWidth(const Font *f, int scale, const char *s) const {
    return gd_text_width(f, scale, s);
}

void GlyphCanvas::IconPlay(int x, int y, int size, u16 color) {
    icon_play(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
              (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
              x, y, size, color);
}

void GlyphCanvas::IconCross(int x, int y, int size, u16 color) {
    icon_cross(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
               (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
               x, y, size, color);
}

void GlyphCanvas::IconButton(int x, int y, int d, char letter, u16 fill, u16 fg,
                             const Font *f) {
    icon_button(m_pDisplay->Buffer(), m_pDisplay->Pitch() / 2,
                (int) m_pDisplay->Width(), (int) m_pDisplay->Height(),
                x, y, d, letter, fill, fg, f);
}
