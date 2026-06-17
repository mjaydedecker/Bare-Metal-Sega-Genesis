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
:   m_pFB(0), m_pBuffer(0), m_Pitch(0), m_LastW(0), m_LastH(0)
{
}

Display::~Display(void)
{
    delete m_pFB;
    m_pFB = 0;
}

boolean Display::Initialize(void)
{
    m_pFB = new CBcmFrameBuffer(FB_WIDTH, FB_HEIGHT, FB_DEPTH);
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
    m_LastW   = 0;
    m_LastH   = 0;
    ClearBlack();
    return TRUE;
}

void Display::ClearBlack(void)
{
    if (m_pBuffer != 0)
    {
        memset(m_pBuffer, 0, (size_t) m_Pitch * FB_HEIGHT);
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

    blit_rgb565(m_pBuffer, m_Pitch, FB_WIDTH, FB_HEIGHT,
                (const uint16_t *) src, (unsigned) pitch, width, height);
}
