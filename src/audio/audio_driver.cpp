//
// src/audio/audio_driver.cpp
//
// Bare Metal Sega Genesis
// See audio_driver.h.
//

#include "audio_driver.h"
#include <circle/sound/soundbasedevice.h>

AudioDriver::AudioDriver(CInterruptSystem *pInterrupt)
:   m_pInterrupt(pInterrupt), m_pDevice(0)
{
}

AudioDriver::~AudioDriver(void)
{
    delete m_pDevice;
    m_pDevice = 0;
}

boolean AudioDriver::Initialize(unsigned nSampleRate)
{
    if (nSampleRate == 0)
    {
        return FALSE;
    }

    m_pDevice = new CHDMISoundBaseDevice(m_pInterrupt, nSampleRate);
    if (m_pDevice == 0)
    {
        return FALSE;
    }

    if (!m_pDevice->AllocateQueue(QUEUE_MS))
    {
        delete m_pDevice;
        m_pDevice = 0;
        return FALSE;
    }

    m_pDevice->SetWriteFormat(SoundFormatSigned16, 2);

    if (!m_pDevice->Start())
    {
        delete m_pDevice;
        m_pDevice = 0;
        return FALSE;
    }

    return m_pDevice->IsActive();
}

void AudioDriver::Write(const s16 *pSamples, unsigned nFrames)
{
    if (m_pDevice != 0 && nFrames > 0)
    {
        // 4 bytes per signed-16 stereo frame; Write() takes a byte count.
        m_pDevice->Write(pSamples, (size_t) nFrames * 4);
    }
}

unsigned AudioDriver::QueuedFrames(void)
{
    return m_pDevice != 0 ? m_pDevice->GetQueueFramesAvail() : 0;
}

boolean AudioDriver::IsReady(void) const
{
    return m_pDevice != 0 && m_pDevice->IsActive();
}
