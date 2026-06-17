//
// src/video/display.h
//
// Bare Metal Sega Genesis
// Owns a fixed 320x240 16bpp framebuffer and blits RGB565 frames into it,
// centered. The Pi firmware GPU-scales the surface to the HDMI output.
//

#ifndef _video_display_h
#define _video_display_h

#include <circle/bcmframebuffer.h>
#include <circle/types.h>

class Display
{
public:
    static const unsigned FB_WIDTH  = 320;
    static const unsigned FB_HEIGHT = 240;   // 320x240 is exactly 4:3
    static const unsigned FB_DEPTH  = 16;    // RGB565

    Display(void);
    ~Display(void);

    boolean Initialize(void);

    // Copy one RGB565 frame, centered. pitch is the source row stride in bytes.
    void Blit(const void *src, unsigned width, unsigned height, size_t pitch);

private:
    void ClearBlack(void);

    CBcmFrameBuffer *m_pFB;
    u16             *m_pBuffer;   // framebuffer base
    unsigned         m_Pitch;     // framebuffer pitch in bytes
    unsigned         m_LastW;
    unsigned         m_LastH;
};

#endif
