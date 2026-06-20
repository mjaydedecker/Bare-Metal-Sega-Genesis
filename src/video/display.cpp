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
:   m_pFB(0), m_pBuffer(0), m_Pitch(0), m_FbW(0), m_FbH(0), m_LastW(0), m_LastH(0),
    m_ScaleMode(ScaleMode::Integer)
{
}

Display::~Display(void)
{
    delete m_pFB;
    m_pFB = 0;
}

boolean Display::Initialize(void)
{
    // Width/height 0 => CBcmFrameBuffer uses the firmware's current display
    // mode, which the TV already accepts (the same mode the M4 console used).
    m_pFB = new CBcmFrameBuffer(0, 0, FB_DEPTH);
    if (m_pFB == 0)
    {
        return FALSE;
    }
    if (!m_pFB->Initialize() || m_pFB->GetBuffer() == 0)
    {
        delete m_pFB;
        m_pFB = 0;
        return FALSE;
    }

    m_pBuffer = (u16 *) (uintptr_t) m_pFB->GetBuffer();
    m_Pitch   = m_pFB->GetPitch();
    m_FbW     = m_pFB->GetWidth();
    m_FbH     = m_pFB->GetHeight();
    m_LastW   = 0;
    m_LastH   = 0;
    ClearBlack();
    return TRUE;
}

void Display::ClearBlack(void)
{
    if (m_pBuffer != 0)
    {
        memset(m_pBuffer, 0, (size_t) m_Pitch * m_FbH);
    }
}

void Display::Blit(const void *src, unsigned width, unsigned height, size_t pitch)
{
    if (m_pBuffer == 0 || src == 0)   // no surface, or dupe frame
    {
        return;
    }

    if (width != m_LastW || height != m_LastH)
    {
        ClearBlack();                 // repaint letterbox/pillarbox bars
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
        blit_rgb565_scaled(m_pBuffer, m_Pitch, m_FbW, m_FbH,
                           (const uint16_t *) src, (unsigned) pitch,
                           width, height, ox, oy, rw, rh);
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

    blit_rgb565(m_pBuffer, m_Pitch, m_FbW, m_FbH,
                (const uint16_t *) src, (unsigned) pitch, width, height, scale);
}
