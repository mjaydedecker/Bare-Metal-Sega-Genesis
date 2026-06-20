#include "../src/audio/audio_util.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // scale_sample
    assert(scale_sample(1000, 100, false) == 1000);   // identity
    assert(scale_sample(1000, 0,   false) == 0);       // zero volume
    assert(scale_sample(1000, 100, true)  == 0);       // mute overrides
    assert(scale_sample(1000, 50,  false) == 500);     // half
    assert(scale_sample(-2000, 50, false) == -1000);   // negative
    assert(scale_sample(32767, 100, false) == 32767);  // max identity
    assert(scale_sample(-32768, 100, false) == -32768);
    assert(scale_sample(-32768, 50, false) == -16384);
    assert(scale_sample(1234, 150, false) == 1234);    // >100 clamps to identity

    // classify_queue (low=0, high=100)
    assert(classify_queue(0,   0, 100) == AQ_Underrun);
    assert(classify_queue(50,  0, 100) == AQ_None);
    assert(classify_queue(100, 0, 100) == AQ_None);    // == high is fine
    assert(classify_queue(101, 0, 100) == AQ_Overrun);
    assert(classify_queue(5,  10, 100) == AQ_Underrun);// low threshold > 0

    printf("All audio_util tests passed\n");
    return 0;
}
