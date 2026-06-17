//
// src/audio/audio_driver.h
//
// Bare Metal Sega Genesis
// Owns an HDMI sound device and feeds it the core's signed-16 stereo samples
// via Circle's Write queue. Knows nothing about libretro.
//

#ifndef _audio_audio_driver_h
#define _audio_audio_driver_h

#include <circle/interrupt.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/types.h>

class AudioDriver
{
public:
    static const unsigned QUEUE_MS = 80;   // queue depth in milliseconds

    AudioDriver(CInterruptSystem *pInterrupt);
    ~AudioDriver(void);

    // Allocate the queue and start the HDMI device at nSampleRate.
    boolean Initialize(unsigned nSampleRate);

    // Push nFrames of interleaved signed-16 stereo samples.
    void Write(const s16 *pSamples, unsigned nFrames);

    // Frames queued but not yet played (the pacing signal).
    unsigned QueuedFrames(void);

    boolean IsReady(void) const;

private:
    CInterruptSystem     *m_pInterrupt;
    CHDMISoundBaseDevice *m_pDevice;
};

#endif
