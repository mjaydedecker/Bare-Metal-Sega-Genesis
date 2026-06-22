//
// src/ui/overlay.cpp
//
// Bare Metal Sega Genesis
// See overlay.h.
//

#include "overlay.h"
#include "theme.h"
#include "fonts/font_vt323_16.h"

// Panel/pill geometry (tuned on hardware if needed).
#define HUD_PAD   8
#define HUD_LH    18    // line height for VT323-16
#define HUD_COLW  108   // column width (two columns -> ~216 px panel)
#define HUD_X     8
#define HUD_Y     8

// Two-column layout for the 9 cells from hud_build (in fixed order).
struct Slot { int row; int col; };
static const Slot kLayout[HUD_CELL_MAX] = {
    {0,0},{0,1},   // FPS | AQ
    {1,0},{1,1},   // UR  | OR
    {2,0},         // ROM  (full width)
    {3,0},         // MODE
    {4,0},{4,1},   // VSYNC | WIDE
    {5,0},         // LAT
};
static const int kRows = 6;

static u16 health_color(HudHealth h)
{
    switch (h)
    {
    case HUD_GOOD: return theme::ACTIVE;     // green
    case HUD_WARN: return theme::ADJUST;     // amber
    case HUD_BAD:  return theme::SELECTION;  // red
    default:       return theme::VALUE;      // cyan (info)
    }
}

static u16 toast_color(ToastKind k)
{
    switch (k)
    {
    case TOAST_SUCCESS: return theme::ACTIVE;
    case TOAST_FAIL:    return theme::SELECTION;
    default:            return theme::VALUE;
    }
}

Overlay::Overlay(GlyphCanvas *pCanvas)
:   m_pCanvas(pCanvas), m_Enabled(false), m_ToastKind(TOAST_INFO), m_ToastFrames(0)
{
    m_Toast[0] = '\0';
}

void Overlay::Draw(const HudStats &s)
{
    if (!m_Enabled) return;

    HudCell cells[HUD_CELL_MAX];
    unsigned n = hud_build(s, cells, HUD_CELL_MAX);

    const Font *f = &g_font_vt323_16;
    int panelW = HUD_PAD * 2 + HUD_COLW * 2;
    int panelH = HUD_PAD * 2 + kRows * HUD_LH;

    // Translucent panel + scanlines (full fixed-size repaint each frame).
    m_pCanvas->BlendRect(HUD_X, HUD_Y, panelW, panelH, theme::BG, 190);
    m_pCanvas->Scanlines(HUD_X, HUD_Y, panelW, panelH, 60);

    for (unsigned i = 0; i < n; i++)
    {
        int x = HUD_X + HUD_PAD + kLayout[i].col * HUD_COLW;
        int y = HUD_Y + HUD_PAD + kLayout[i].row * HUD_LH;
        // label (muted) then value (health color), flowing left-to-right.
        int vx = m_pCanvas->Text(f, 1, x, y, cells[i].label,
                                 theme::TEXT_MUTED, 0, true);
        vx += f->width;  // one-char gap
        m_pCanvas->Text(f, 1, vx, y, cells[i].value,
                        health_color(cells[i].health), 0, true);
    }
}

void Overlay::ShowToast(const char *msg, ToastKind kind)
{
    unsigned i = 0;
    if (msg != 0)
        for (; msg[i] != '\0' && i < TOAST_MAX; i++) m_Toast[i] = msg[i];
    m_Toast[i]    = '\0';
    m_ToastKind   = kind;
    m_ToastFrames = TOAST_FRAMES;
}

void Overlay::DrawToast(void)
{
    if (m_ToastFrames == 0) return;
    m_ToastFrames--;

    const Font *f = &g_font_vt323_16;
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();

    int padX = 12, padY = 6;
    int textW = m_pCanvas->TextWidth(f, 1, m_Toast);
    int boxW  = textW + 2 * padX;
    int boxH  = (int) f->height + 2 * padY;
    int x = (W - boxW) / 2; if (x < 0) x = 0;
    int y = H - boxH - 36;  // near bottom (HUD is top-left)

    // Translucent pill (full repaint each frame).
    m_pCanvas->BlendRect(x, y, boxW, boxH, theme::BG, 200);
    m_pCanvas->Scanlines(x, y, boxW, boxH, 60);
    m_pCanvas->Text(f, 1, x + padX, y + padY, m_Toast,
                    toast_color(m_ToastKind), 0, true);
}
