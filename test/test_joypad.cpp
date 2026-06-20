#include "../src/input/joypad_map.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // Single buttons map correctly.
    assert(joypad_state(GP_A,      RETRO_DEVICE_ID_JOYPAD_A)      == 1);
    assert(joypad_state(GP_B,      RETRO_DEVICE_ID_JOYPAD_B)      == 1);
    assert(joypad_state(GP_X,      RETRO_DEVICE_ID_JOYPAD_X)      == 1);
    assert(joypad_state(GP_Y,      RETRO_DEVICE_ID_JOYPAD_Y)      == 1);
    assert(joypad_state(GP_LB,     RETRO_DEVICE_ID_JOYPAD_L)      == 1);
    assert(joypad_state(GP_RB,     RETRO_DEVICE_ID_JOYPAD_R)      == 1);
    assert(joypad_state(GP_START,  RETRO_DEVICE_ID_JOYPAD_START)  == 1);
    assert(joypad_state(GP_SELECT, RETRO_DEVICE_ID_JOYPAD_SELECT) == 1);
    assert(joypad_state(GP_UP,     RETRO_DEVICE_ID_JOYPAD_UP)     == 1);
    assert(joypad_state(GP_DOWN,   RETRO_DEVICE_ID_JOYPAD_DOWN)   == 1);
    assert(joypad_state(GP_LEFT,   RETRO_DEVICE_ID_JOYPAD_LEFT)   == 1);
    assert(joypad_state(GP_RIGHT,  RETRO_DEVICE_ID_JOYPAD_RIGHT)  == 1);

    // A pressed button does not register as a different one.
    assert(joypad_state(GP_A, RETRO_DEVICE_ID_JOYPAD_B) == 0);

    // Multiple buttons at once resolve independently.
    unsigned combo = GP_A | GP_DOWN | GP_RB;
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_A)    == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_DOWN) == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_R)    == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_B)    == 0);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_UP)   == 0);

    // Empty bitmask and unmapped id.
    assert(joypad_state(0,          RETRO_DEVICE_ID_JOYPAD_A) == 0);
    assert(joypad_state(0xFFFFFFFF, 999u)                     == 0);

    // JOYPAD_MASK returns a bitmask of pressed button ids (this is how the
    // core actually reads the pad).
    int16_t m = joypad_state(GP_A | GP_START | GP_DOWN, RETRO_DEVICE_ID_JOYPAD_MASK);
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_A));
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_START));
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));
    assert(!(m & (1 << RETRO_DEVICE_ID_JOYPAD_B)));
    assert(!(m & (1 << RETRO_DEVICE_ID_JOYPAD_UP)));
    assert(joypad_state(0, RETRO_DEVICE_ID_JOYPAD_MASK) == 0);

    printf("All joypad tests passed\n");
    return 0;
}
