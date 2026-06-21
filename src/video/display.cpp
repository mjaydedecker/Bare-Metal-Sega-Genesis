//
// src/video/display.cpp
//
// Bare Metal Sega Genesis
// See display.h.
//

#include "display.h"
#include "blit.h"
#include <circle/util.h>
#include <stdint.h>

Display::Display(void)
:   m_pFB(0), m_pFront(0), m_pBack(0), m_FrontIsPage0(true),
    m_DoubleBuffered(false), m_Vsync(true),
    m_Pitch(0), m_FbW(0), m_FbH(0), m_LastW(0), m_LastH(0),
    m_ScaleMode(ScaleMode::Integer)
{
}

Display::~Display(void)
{
    delete m_pFB;
    m_pFB = 0;
}

// Build a framebuffer for the requested mode (0,0 => firmware's current mode),
// preferring a two-page (double-height virtual) surface for tear-free flipping.
// Returns 0 on total failure; sets `doubled` to whether two pages were obtained.
CBcmFrameBuffer *Display::BuildFB(unsigned reqW, unsigned reqH, bool &doubled)
{
    unsigned w = reqW, h = reqH;

    // For the firmware's current mode (0,0) we must learn its dimensions before
    // we can request a double-height virtual framebuffer.
    if (w == 0 || h == 0)
    {
        CBcmFrameBuffer *probe = new CBcmFrameBuffer(reqW, reqH, FB_DEPTH);
        if (probe != 0 && probe->Initialize() && probe->GetBuffer() != 0)
        {
            w = probe->GetWidth();
            h = probe->GetHeight();
        }
        delete probe;
        if (w == 0 || h == 0)
        {
            doubled = false;
            return 0;
        }
    }

    // Prefer two pages: virtual height = 2x physical.
    CBcmFrameBuffer *fb = new CBcmFrameBuffer(w, h, FB_DEPTH, w, 2 * h);
    if (fb != 0 && fb->Initialize() && fb->GetBuffer() != 0)
    {
        doubled = true;
        return fb;
    }
    delete fb;

    // Fall back to a single page (today's behavior).
    fb = new CBcmFrameBuffer(w, h, FB_DEPTH);
    if (fb != 0 && fb->Initialize() && fb->GetBuffer() != 0)
    {
        doubled = false;
        return fb;
    }
    delete fb;
    doubled = false;
    return 0;
}

// Wire the display state to a freshly-built framebuffer. The new FB defaults to
// virtual offset (0,0), so page 0 is visible (front).
void Display::Adopt(CBcmFrameBuffer *fb, bool doubled)
{
    m_pFB            = fb;
    m_Pitch          = fb->GetPitch();
    m_FbW            = fb->GetWidth();
    m_FbH            = fb->GetHeight();
    m_DoubleBuffered = doubled;
    m_FrontIsPage0   = true;

    u16 *page0 = (u16 *) (uintptr_t) fb->GetBuffer();
    m_pFront = page0;
    m_pBack  = doubled
        ? (u16 *) ((u8 *) page0 + (size_t) m_Pitch * m_FbH)
        : page0;

    m_LastW = 0;
    m_LastH = 0;
    ClearBlack();
}

boolean Display::Initialize(void)
{
    bool doubled = false;
    CBcmFrameBuffer *fb = BuildFB(0, 0, doubled);
    if (fb == 0)
    {
        return FALSE;
    }
    Adopt(fb, doubled);
    return TRUE;
}

boolean Display::SetMode(unsigned w, unsigned h)
{
    // Build the new framebuffer BEFORE discarding the current one, so a failure
    // leaves the existing mode intact.
    bool doubled = false;
    CBcmFrameBuffer *pNew = BuildFB(w, h, doubled);
    if (pNew == 0)
    {
        return FALSE;
    }
    delete m_pFB;
    Adopt(pNew, doubled);
    return TRUE;
}

void Display::ClearBlack(void)
{
    if (m_pFront != 0)
    {
        memset(m_pFront, 0, (size_t) m_Pitch * m_FbH);
    }
    if (m_DoubleBuffered && m_pBack != 0)
    {
        memset(m_pBack, 0, (size_t) m_Pitch * m_FbH);
    }
}

// Flip at vblank: wait for vsync, pan to the page we just drew (the back page),
// then swap front/back so the next Blit targets the now-hidden page.
void Display::Present(void)
{
    m_pFB->WaitForVerticalSync();
    m_pFB->SetVirtualOffset(0, m_FrontIsPage0 ? m_FbH : 0);

    u16 *tmp = m_pFront;
    m_pFront = m_pBack;
    m_pBack  = tmp;
    m_FrontIsPage0 = !m_FrontIsPage0;
}

void Display::Blit(const void *src, unsigned width, unsigned height, size_t pitch)
{
    if (m_pFront == 0 || src == 0)   // no surface, or dupe frame
    {
        return;
    }

    bool flip   = m_Vsync && m_DoubleBuffered;
    u16 *target = flip ? m_pBack : m_pFront;

    if (width != m_LastW || height != m_LastH)
    {
        ClearBlack();                 // both pages: repaint letterbox/pillarbox
        m_LastW = width;
        m_LastH = height;
    }

    if (m_ScaleMode == ScaleMode::Stretch && width != 0 && height != 0)
    {
        // Largest 4:3 rectangle that fits the framebuffer, centered; the frame
        // is stretched (non-integer) to fill it.
        unsigned rw = m_FbW;
        unsigned rh = m_FbW * 3 / 4;
        if (rh > m_FbH) { rh = m_FbH; rw = m_FbH * 4 / 3; }
        unsigned ox = (m_FbW - rw) / 2;
        unsigned oy = (m_FbH - rh) / 2;
        blit_rgb565_scaled(target, m_Pitch, m_FbW, m_FbH,
                           (const uint16_t *) src, (unsigned) pitch,
                           width, height, ox, oy, rw, rh);
        if (flip) Present();
        return;
    }

    // Integer mode: largest whole scale that fits the framebuffer.
    unsigned scale = 1;
    if (width != 0 && height != 0)
    {
        unsigned sx = m_FbW / width;
        unsigned sy = m_FbH / height;
        scale = (sx < sy) ? sx : sy;
        if (scale < 1)
        {
            scale = 1;
        }
    }

    blit_rgb565(target, m_Pitch, m_FbW, m_FbH,
                (const uint16_t *) src, (unsigned) pitch, width, height, scale);
    if (flip) Present();
}
