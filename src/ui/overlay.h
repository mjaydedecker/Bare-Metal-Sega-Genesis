//
// src/ui/overlay.h
//
// Bare Metal Sega Genesis
// Composites the diagnostics HUD + toasts onto the game framebuffer via
// GlyphCanvas (v1.0.0 look). Drawn after retro_run() each frame; no-op when
// disabled. Does not touch Display::Blit/Present.
//

#ifndef _ui_overlay_h
#define _ui_overlay_h

#include "glyph_canvas.h"
#include "hud.h"

#define TOAST_MAX    28
#define TOAST_FRAMES 120   // ~2 s at 60 fps

enum ToastKind { TOAST_INFO, TOAST_SUCCESS, TOAST_FAIL };

class Overlay
{
public:
    explicit Overlay(GlyphCanvas *pCanvas);

    void SetEnabled(bool on) { m_Enabled = on; }
    bool Enabled(void) const { return m_Enabled; }

    void Draw(const HudStats &s);   // no-op if disabled

    void ShowToast(const char *msg, ToastKind kind = TOAST_INFO);
    void DrawToast(void);

private:
    GlyphCanvas *m_pCanvas;
    bool         m_Enabled;
    char         m_Toast[TOAST_MAX + 1];
    ToastKind    m_ToastKind;
    unsigned     m_ToastFrames;
};

#endif
