//
// src/audio/audio_driver.cpp
//
// Bare Metal Sega Genesis
// See audio_driver.h.
//

#include "audio_driver.h"
#include "audio_util.h"
#include <circle/sound/soundbasedevice.h>

AudioDriver::AudioDriver(CInterruptSystem *pInterrupt)
:   m_pInterrupt(pInterrupt), m_pDevice(0),
    m_Volume(100), m_Mute(false), m_Underruns(0), m_Overruns(0)
{
}

void AudioDriver::SetVolume(unsigned volume)
{
    m_Volume = volume > 100 ? 100 : volume;
}

void AudioDriver::SetMute(bool mute)
{
    m_Mute = mute;
}

AudioDriver::~AudioDriver(void)
{
    delete m_pDevice;
    m_pDevice = 0;
}

boolean AudioDriver::Initialize(unsigned nSampleRate, AudioOutput out)
{
    if (nSampleRate == 0)
    {
        return FALSE;
    }

    if (out == AudioOutput::Analog)
        m_pDevice = new CPWMSoundBaseDevice(m_pInterrupt, nSampleRate);
    else if (out == AudioOutput::I2S)
        // PCM5102-class DAC: Pi is I2S master, no codec I2C init needed, so all
        // I2S-specific ctor args (nChunkSize, bSlave, pI2CMaster) take defaults.
        m_pDevice = new CI2SSoundBaseDevice(m_pInterrupt, nSampleRate);
    else
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
    if (m_pDevice == 0 || nFrames == 0)
    {
        return;
    }

    // Fast path: full volume, not muted -> write the core's buffer directly.
    if (!m_Mute && m_Volume >= 100)
    {
        m_pDevice->Write(pSamples, (size_t) nFrames * 4);
        return;
    }

    // Scaled / muted path: stage gained samples in chunks (2 s16 per frame).
    // Always writes nFrames (silence when muted) so queue pacing is unaffected.
    static s16 staging[STAGE_FRAMES * 2];
    unsigned done = 0;
    while (done < nFrames)
    {
        unsigned chunk = nFrames - done;
        if (chunk > STAGE_FRAMES) chunk = STAGE_FRAMES;

        const s16 *in = pSamples + (size_t) done * 2;
        for (unsigned i = 0; i < chunk * 2; i++)
        {
            staging[i] = scale_sample(in[i], m_Volume, m_Mute);
        }

        m_pDevice->Write(staging, (size_t) chunk * 4);
        done += chunk;
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
