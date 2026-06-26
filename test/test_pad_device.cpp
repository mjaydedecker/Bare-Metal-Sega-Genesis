#include "../src/input/pad_device.h"
#include "../src/input/sega_pad.h"
#include "../src/settings/settings.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // A live GPIO detection wins over the setting, both ways.
    assert(pad_use_6button(SegaPadType::SixButton,   PadType::ThreeButton) == true);
    assert(pad_use_6button(SegaPadType::SixButton,   PadType::SixButton)   == true);
    assert(pad_use_6button(SegaPadType::ThreeButton, PadType::SixButton)   == false);
    assert(pad_use_6button(SegaPadType::ThreeButton, PadType::ThreeButton) == false);

    // No usable GPIO pad (None / Unsupported): fall back to the global setting.
    assert(pad_use_6button(SegaPadType::None,        PadType::SixButton)   == true);
    assert(pad_use_6button(SegaPadType::None,        PadType::ThreeButton) == false);
    assert(pad_use_6button(SegaPadType::Unsupported, PadType::SixButton)   == true);
    assert(pad_use_6button(SegaPadType::Unsupported, PadType::ThreeButton) == false);

    printf("test_pad_device: OK\n");
    return 0;
}
