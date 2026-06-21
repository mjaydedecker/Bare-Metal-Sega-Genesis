//
// src/video/display.h
//
// Bare Metal Sega Genesis
// Owns a framebuffer at the firmware's current (TV-supported) display mode and
// blits RGB565 frames into it, nearest-neighbor integer-scaled and centered.
//
// NOTE: on the Pi mailbox framebuffer the physical size IS the HDMI output
// resolution (the firmware does not scale a small framebuffer up to the panel).
// Requesting a tiny mode like 320x240 produces an "unsupported signal" on most
// TVs, so we take the current display mode and scale the frame on the CPU.
//

#ifndef _video_display_h
#define _video_display_h

#include <circle/bcmframebuffer.h>
#include <circle/types.h>
#include "../settings/settings.h"   // ScaleMode

class Display
{
public:
    static const unsigned FB_DEPTH = 16;     // RGB565

    Display(void);
    ~Display(void);

    boolean Initialize(void);

    // Re-create the framebuffer at a new HDMI mode (w=h=0 => firmware native).
    // On success swaps to the new mode and returns TRUE; on failure keeps the
    // current mode and returns FALSE. NOTE: success does not prove the TV
    // accepts the signal — callers must confirm-or-revert.
    boolean SetMode(unsigned w, unsigned h);

    // Select integer (sharp, letterboxed) vs stretch (aspect-fill) scaling.
    void SetScaleMode(ScaleMode mode) { m_ScaleMode = mode; }

    // Enable/disable the tear-free vsync page flip. Live; takes effect on the
    // next Blit. When off (or if double-buffering is unavailable) Blit draws
    // straight to the visible page with no flip.
    void SetVsync(bool on) { m_Vsync = on; }

    // Copy one RGB565 frame, integer-scaled and centered. pitch is the source
    // row stride in bytes.
    void Blit(const void *src, unsigned width, unsigned height, size_t pitch);

    // Framebuffer accessors for on-screen UI (TextCanvas draws into the visible
    // page). Game frames go through Blit(), never Buffer().
    u16     *Buffer(void) const { return m_pFront; }
    unsigned Pitch (void) const { return m_Pitch; }   // bytes per row
    unsigned Width (void) const { return m_FbW; }     // pixels
    unsigned Height(void) const { return m_FbH; }     // pixels

    // Force the next Blit to clear the framebuffer (repaint letterbox bars).
    void ForceRepaint(void) { m_LastW = 0; m_LastH = 0; }

private:
    void ClearBlack(void);                 // clears both pages when double-buffered
    void Present(void);                    // vsync flip + swap front/back
    CBcmFrameBuffer *BuildFB(unsigned reqW, unsigned reqH, bool &doubled);
    void Adopt(CBcmFrameBuffer *fb, bool doubled);

    CBcmFrameBuffer *m_pFB;
    u16             *m_pFront;        // visible page base (UI + non-vsync draw)
    u16             *m_pBack;         // off-screen page base (== front if single)
    bool             m_FrontIsPage0;  // which physical page is currently visible
    bool             m_DoubleBuffered;
    bool             m_Vsync;
    unsigned         m_Pitch;         // framebuffer pitch in bytes
    unsigned         m_FbW;           // framebuffer width in pixels
    unsigned         m_FbH;           // framebuffer height in pixels
    unsigned         m_LastW;
    unsigned         m_LastH;
    ScaleMode        m_ScaleMode;
};

#endif
