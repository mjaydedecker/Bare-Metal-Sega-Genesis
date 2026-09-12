//
// src/ui/overlay.cpp
//
// Bare Metal Sega Genesis
// See overlay.h.
//

#include "overlay.h"
#include "theme.h"
#include "fonts/font_vt323_22.h"

// Geometry is in GlyphCanvas logical units; the canvas applies the
// resolution-based UI scale (ui_scale), so the HUD grows with the menus and
// reads clearly at 1080p and above.
static const Font *kHudFont = &g_font_vt323_22;
#define HUD_MARGIN  8     // panel offset from top-left corner (logical px)
#define HUD_PAD     8     // inner padding (logical px)
#define HUD_GAP     4     // extra px between text rows (logical px)
#define HUD_COLCH   13    // column width in characters (two columns)

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

    const Font *f  = kHudFont;
    int fw   = (int) f->width;
    int lh   = (int) f->height + HUD_GAP;
    int pad  = HUD_PAD;
    int colW = HUD_COLCH * fw;
    int ox   = HUD_MARGIN;
    int oy   = HUD_MARGIN;
    int panelW = pad * 2 + colW * 2;
    int panelH = pad * 2 + kRows * lh;

    // Pseudo-translucent panel via write-only stipple (no framebuffer reads —
    // blend/scanline read-modify-write stalls the Pi's write-combining FB and
    // cost ~8 fps + audio distortion with the HUD on). Full repaint each frame.
    m_pCanvas->StippleRect(ox, oy, panelW, panelH, theme::BG);

    for (unsigned i = 0; i < n; i++)
    {
        int x = ox + pad + kLayout[i].col * colW;
        int y = oy + pad + kLayout[i].row * lh;
        // label (muted) then value (health color), flowing left-to-right.
        int vx = m_pCanvas->Text(f, 1, x, y, cells[i].label,
                                 theme::TEXT_MUTED, 0, true);
        vx += fw;  // one-char gap
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

    const Font *f = kHudFont;
    int W  = (int) m_pCanvas->Width();
    int H  = (int) m_pCanvas->Height();

    int padX = 12, padY = 6;
    int textW = m_pCanvas->TextWidth(f, 1, m_Toast);
    int boxW  = textW + 2 * padX;
    int boxH  = (int) f->height + 2 * padY;
    int x = (W - boxW) / 2; if (x < 0) x = 0;
    int y = H - boxH - 36;  // near bottom (HUD is top-left)

    // Pseudo-translucent pill via write-only stipple (no framebuffer reads).
    m_pCanvas->StippleRect(x, y, boxW, boxH, theme::BG);
    m_pCanvas->Text(f, 1, x + padX, y + padY, m_Toast,
                    toast_color(m_ToastKind), 0, true);
}
