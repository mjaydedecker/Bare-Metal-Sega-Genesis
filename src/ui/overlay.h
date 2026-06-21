//
// src/ui/overlay.h
//
// Bare Metal Sega Genesis
// Composites the diagnostics HUD onto the game framebuffer via TextCanvas.
// Drawn after retro_run() each frame; no-op when disabled.
//

#ifndef _ui_overlay_h
#define _ui_overlay_h

#include "text_canvas.h"
#include "hud.h"

#define TOAST_MAX    28
#define TOAST_FRAMES 120   // ~2 s at 60 fps

class Overlay
{
public:
    explicit Overlay(TextCanvas *pCanvas);

    void SetEnabled(bool on) { m_Enabled = on; }
    bool Enabled(void) const { return m_Enabled; }

    void Draw(const HudStats &s);   // no-op if disabled

    void ShowToast(const char *msg);  // start a transient bottom-center message
    void DrawToast(void);             // every frame: draw if active, then decay

private:
    TextCanvas *m_pCanvas;
    bool        m_Enabled;
    char        m_Toast[TOAST_MAX + 1];
    unsigned    m_ToastFrames;
};

#endif
