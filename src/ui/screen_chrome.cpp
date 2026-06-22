//
// src/ui/screen_chrome.cpp
//
// Bare Metal Sega Genesis
// See screen_chrome.h.
//
#include "screen_chrome.h"
#include "theme.h"
#include "fonts/font_ps2p8.h"
#include "fonts/font_vt323_22.h"

namespace chrome {

void header(GlyphCanvas *c, const char *title, const char *right, u16 rightColor) {
    int W = (int) c->Width();
    c->Text(&g_font_ps2p8, 2, PAD, TOP, title, theme::WHITE, theme::BG, true);
    if (right && right[0]) {
        int rw = c->TextWidth(&g_font_vt323_22, 1, right);
        c->Text(&g_font_vt323_22, 1, W - PAD - rw, TOP, right, rightColor,
                theme::BG, true);
    }
    c->FillRect(PAD, TOP + 34, W - 2 * PAD, 2, theme::TEXT_DIM);
}

void footer_divider(GlyphCanvas *c) {
    int W = (int) c->Width();
    int H = (int) c->Height();
    c->FillRect(PAD, H - FOOT_H - 2, W - 2 * PAD, 2, theme::TEXT_DIM);
}

void scanlines(GlyphCanvas *c) {
    c->Scanlines(0, 0, (int) c->Width(), (int) c->Height(), 60);
}

int value_row(GlyphCanvas *c, int i, bool sel, const char *label,
              const char *value, u16 valueColor, bool arrows) {
    int W = (int) c->Width();
    int y = LIST_TOP + i * ROW_H;
    int rowW = W - 2 * PAD;
    if (sel) c->FillRect(PAD - 6, y - 2, rowW + 12, ROW_H, theme::SELECTION);

    u16 lc = sel ? theme::WHITE : theme::TEXT;
    c->Text(&g_font_vt323_22, 1, PAD + 4, y, label, lc, theme::BG, true);

    if (value && value[0]) {
        int vw = c->TextWidth(&g_font_vt323_22, 1, value);
        u16 vc = sel ? theme::WHITE : valueColor;
        u16 ac = sel ? theme::WHITE : theme::ADJUST;
        if (arrows) {
            int rax = W - PAD - 12;          // right arrow box
            int vx  = rax - 6 - vw;          // value sits left of it
            int lax = vx - 18;               // left arrow box
            c->IconTri(lax, y + 5, 12, 1, ac);
            c->Text(&g_font_vt323_22, 1, vx, y, value, vc, theme::BG, true);
            c->IconTri(rax, y + 5, 12, 0, ac);
        } else {
            c->Text(&g_font_vt323_22, 1, W - PAD - vw, y, value, vc,
                    theme::BG, true);
        }
    }
    return y;
}

int hint_dpad(GlyphCanvas *c, int x, int y, const char *label) {
    c->IconCross(x, y, 16, theme::TEXT);
    c->Text(&g_font_ps2p8, 1, x + 24, y + 4, label, theme::TEXT_DIM, theme::BG, true);
    return x + 24 + c->TextWidth(&g_font_ps2p8, 1, label) + 30;
}

int hint_lr(GlyphCanvas *c, int x, int y, const char *label) {
    c->IconTri(x, y + 4, 12, 1, theme::ADJUST);
    c->IconTri(x + 16, y + 4, 12, 0, theme::ADJUST);
    c->Text(&g_font_ps2p8, 1, x + 36, y + 4, label, theme::TEXT_DIM, theme::BG, true);
    return x + 36 + c->TextWidth(&g_font_ps2p8, 1, label) + 30;
}

int hint_button(GlyphCanvas *c, int x, int y, char btn, u16 fill, const char *label) {
    c->IconButton(x, y, 16, btn, fill, theme::WHITE, &g_font_ps2p8);
    c->Text(&g_font_ps2p8, 1, x + 24, y + 4, label, theme::TEXT_DIM, theme::BG, true);
    return x + 24 + c->TextWidth(&g_font_ps2p8, 1, label) + 30;
}

}  // namespace chrome
