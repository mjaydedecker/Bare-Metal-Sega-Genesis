//
// src/audio/audio_util.cpp
//
// Bare Metal Sega Genesis
// See audio_util.h.
//

#include "audio_util.h"

int16_t scale_sample(int16_t sample, unsigned volume, bool mute)
{
    if (mute || volume == 0) return 0;
    if (volume >= 100)       return sample;
    return (int16_t) (((int32_t) sample * (int32_t) volume) / 100);
}

AudioQueueEvent classify_queue(unsigned frames, unsigned low, unsigned high)
{
    if (frames <= low) return AQ_Underrun;
    if (frames > high) return AQ_Overrun;
    return AQ_None;
}
