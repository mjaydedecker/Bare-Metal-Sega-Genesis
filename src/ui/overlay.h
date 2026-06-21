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

class Overlay
{
public:
    explicit Overlay(TextCanvas *pCanvas);

    void SetEnabled(bool on) { m_Enabled = on; }
    bool Enabled(void) const { return m_Enabled; }

    void Draw(const HudStats &s);   // no-op if disabled

private:
    TextCanvas *m_pCanvas;
    bool        m_Enabled;
};

#endif
