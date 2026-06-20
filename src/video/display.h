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

class Display
{
public:
    static const unsigned FB_DEPTH = 16;     // RGB565

    Display(void);
    ~Display(void);

    boolean Initialize(void);

    // Copy one RGB565 frame, integer-scaled and centered. pitch is the source
    // row stride in bytes.
    void Blit(const void *src, unsigned width, unsigned height, size_t pitch);

    // Framebuffer accessors for on-screen UI (TextCanvas draws into this).
    u16     *Buffer(void) const { return m_pBuffer; }
    unsigned Pitch (void) const { return m_Pitch; }   // bytes per row
    unsigned Width (void) const { return m_FbW; }     // pixels
    unsigned Height(void) const { return m_FbH; }     // pixels

    // Force the next Blit to clear the whole framebuffer (repaint the letterbox
    // bars). Used after an on-screen overlay (e.g. the pause menu) drew over the
    // bars, which Blit otherwise only repaints on a frame-size change.
    void ForceRepaint(void) { m_LastW = 0; m_LastH = 0; }

private:
    void ClearBlack(void);

    CBcmFrameBuffer *m_pFB;
    u16             *m_pBuffer;   // framebuffer base
    unsigned         m_Pitch;     // framebuffer pitch in bytes
    unsigned         m_FbW;       // framebuffer width in pixels (native mode)
    unsigned         m_FbH;       // framebuffer height in pixels (native mode)
    unsigned         m_LastW;
    unsigned         m_LastH;
};

#endif
