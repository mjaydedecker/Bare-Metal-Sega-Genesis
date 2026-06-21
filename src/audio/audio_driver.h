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
#include <circle/sound/soundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/types.h>
#include "../settings/settings.h"   // AudioOutput

class AudioDriver
{
public:
    static const unsigned QUEUE_MS = 80;   // queue depth in milliseconds

    AudioDriver(CInterruptSystem *pInterrupt);
    ~AudioDriver(void);

    // Allocate the queue and start the sound device at nSampleRate.
    // out selects the physical output (HDMI or the 3.5mm analog jack).
    boolean Initialize(unsigned nSampleRate, AudioOutput out);

    // Push nFrames of interleaved signed-16 stereo samples.
    void Write(const s16 *pSamples, unsigned nFrames);

    // Frames queued but not yet played (the pacing signal).
    unsigned QueuedFrames(void);

    boolean IsReady(void) const;

    // Master volume 0-100 and mute. Safe to call before Initialize() — the
    // values are applied when Write() runs.
    void SetVolume(unsigned volume);   // clamped to 0-100
    void SetMute(bool mute);

    // Underrun/overrun metrics (bumped by the kernel from the pacing loop).
    void     RecordUnderrun(void) { m_Underruns++; }
    void     RecordOverrun (void) { m_Overruns++;  }
    unsigned Underruns(void) const { return m_Underruns; }
    unsigned Overruns (void) const { return m_Overruns;  }

private:
    static const unsigned STAGE_FRAMES = 1024;   // gain staging chunk size

    CInterruptSystem     *m_pInterrupt;
    CSoundBaseDevice     *m_pDevice;
    unsigned              m_Volume;     // 0-100
    bool                  m_Mute;
    unsigned              m_Underruns;
    unsigned              m_Overruns;
};

#endif
