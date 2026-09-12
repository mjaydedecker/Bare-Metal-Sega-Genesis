#include "../src/ui/ui_scale.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // Standard HDMI modes: whole-number steps, logical space >= 640x480.
    assert(ui_scale(720,  480)  == 1);   // 480p
    assert(ui_scale(1280, 720)  == 1);   // 720p
    assert(ui_scale(1920, 1080) == 2);   // 1080p -> 960x540
    assert(ui_scale(2560, 1440) == 3);   // 1440p -> 853x480
    assert(ui_scale(3840, 2160) == 4);   // 4K    -> 960x540

    // Non-16:9 native modes.
    assert(ui_scale(1280, 1024) == 2);   // -> 640x512
    assert(ui_scale(1024, 768)  == 1);
    assert(ui_scale(1024, 1024) == 1);   // width-limited: height alone gives 2

    // Degenerate / tiny sizes never return 0.
    assert(ui_scale(0, 0)       == 1);
    assert(ui_scale(320, 240)   == 1);
    assert(ui_scale(1920, 0)    == 1);

    printf("test_ui_scale: OK\n");
    return 0;
}
