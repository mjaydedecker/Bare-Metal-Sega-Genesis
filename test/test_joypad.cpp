#include "../src/input/joypad_map.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    ButtonMap dm;   // default map (reproduces historical behavior)

    // Single buttons map correctly under the default map.
    assert(joypad_state(GP_A,      RETRO_DEVICE_ID_JOYPAD_A, dm)      == 1);
    assert(joypad_state(GP_B,      RETRO_DEVICE_ID_JOYPAD_B, dm)      == 1);
    assert(joypad_state(GP_X,      RETRO_DEVICE_ID_JOYPAD_X, dm)      == 1);
    assert(joypad_state(GP_Y,      RETRO_DEVICE_ID_JOYPAD_Y, dm)      == 1);
    assert(joypad_state(GP_LB,     RETRO_DEVICE_ID_JOYPAD_L, dm)      == 1);
    assert(joypad_state(GP_RB,     RETRO_DEVICE_ID_JOYPAD_R, dm)      == 1);
    assert(joypad_state(GP_START,  RETRO_DEVICE_ID_JOYPAD_START, dm)  == 1);
    assert(joypad_state(GP_SELECT, RETRO_DEVICE_ID_JOYPAD_SELECT, dm) == 1);
    assert(joypad_state(GP_UP,     RETRO_DEVICE_ID_JOYPAD_UP, dm)     == 1);
    assert(joypad_state(GP_DOWN,   RETRO_DEVICE_ID_JOYPAD_DOWN, dm)   == 1);
    assert(joypad_state(GP_LEFT,   RETRO_DEVICE_ID_JOYPAD_LEFT, dm)   == 1);
    assert(joypad_state(GP_RIGHT,  RETRO_DEVICE_ID_JOYPAD_RIGHT, dm)  == 1);

    // A pressed button does not register as a different one.
    assert(joypad_state(GP_A, RETRO_DEVICE_ID_JOYPAD_B, dm) == 0);

    // Multiple buttons at once resolve independently.
    unsigned combo = GP_A | GP_DOWN | GP_RB;
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_A, dm)    == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_DOWN, dm) == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_R, dm)    == 1);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_B, dm)    == 0);
    assert(joypad_state(combo, RETRO_DEVICE_ID_JOYPAD_UP, dm)   == 0);

    // Empty bitmask and unmapped id.
    assert(joypad_state(0,          RETRO_DEVICE_ID_JOYPAD_A, dm) == 0);
    assert(joypad_state(0xFFFFFFFF, 999u, dm)                     == 0);

    // JOYPAD_MASK returns a bitmask of pressed button ids.
    int16_t m = joypad_state(GP_A | GP_START | GP_DOWN, RETRO_DEVICE_ID_JOYPAD_MASK, dm);
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_A));
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_START));
    assert(m & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));
    assert(!(m & (1 << RETRO_DEVICE_ID_JOYPAD_B)));
    assert(!(m & (1 << RETRO_DEVICE_ID_JOYPAD_UP)));
    assert(joypad_state(0, RETRO_DEVICE_ID_JOYPAD_MASK, dm) == 0);

    // Two pads combine for menu navigation as a bitwise OR.
    unsigned combined = (GP_UP) | (GP_START);
    assert(joypad_state(combined, RETRO_DEVICE_ID_JOYPAD_UP, dm));
    assert(joypad_state(combined, RETRO_DEVICE_ID_JOYPAD_START, dm));

    // Remapping: physical A now drives Genesis A (id JOYPAD_Y).
    ButtonMap rm;
    rm.b[0] = PadButton::A;
    assert(joypad_state(GP_A, RETRO_DEVICE_ID_JOYPAD_Y, rm) == 1);
    assert(joypad_state(GP_Y, RETRO_DEVICE_ID_JOYPAD_Y, rm) == 0);
    // D-pad stays fixed regardless of the map.
    assert(joypad_state(GP_UP, RETRO_DEVICE_ID_JOYPAD_UP, rm) == 1);

    // pad_bit mapping.
    assert(pad_bit(PadButton::A) == GP_A);
    assert(pad_bit(PadButton::L) == GP_LB);
    assert(pad_bit(PadButton::Select) == GP_SELECT);

    // Menu-hotkey preset -> button bitmask.
    assert(hotkey_mask(MenuHotkey::StartSelect) == (GP_START | GP_SELECT));
    assert(hotkey_mask(MenuHotkey::StartA)      == (GP_START | GP_A));
    assert(hotkey_mask(MenuHotkey::StartB)      == (GP_START | GP_B));
    assert(hotkey_mask(MenuHotkey::LR)          == (GP_LB | GP_RB));

    printf("All joypad tests passed\n");
    return 0;
}
