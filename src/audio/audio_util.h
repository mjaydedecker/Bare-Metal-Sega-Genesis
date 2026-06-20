//
// src/audio/audio_util.h
//
// Bare Metal Sega Genesis
// Pure audio helpers: per-sample volume/mute gain and audio-queue-depth
// classification. No Circle dependencies — host-testable.
//

#ifndef _audio_audio_util_h
#define _audio_audio_util_h

#include <stdint.h>

// Scale one signed-16 audio sample by a 0-100 volume percentage. mute or
// volume==0 yields 0; volume>=100 returns the sample unchanged. Scaling down
// never overflows int16.
int16_t scale_sample(int16_t sample, unsigned volume, bool mute);

enum AudioQueueEvent { AQ_None, AQ_Underrun, AQ_Overrun };

// Classify an audio-queue depth: frames <= low => AQ_Underrun (starved),
// frames > high => AQ_Overrun (too full), otherwise AQ_None.
AudioQueueEvent classify_queue(unsigned frames, unsigned low, unsigned high);

#endif
