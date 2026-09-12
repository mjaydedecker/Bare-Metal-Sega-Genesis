#include "../src/menu/pad_test.h"
#include "../src/input/joypad_map.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static const PadTestKey *key(const char *label)
{
    for (unsigned i = 0; i < PAD_TEST_NUM_KEYS; i++)
        if (strcmp(PAD_TEST_KEYS[i].label, label) == 0) return &PAD_TEST_KEYS[i];
    return 0;
}

int main(void)
{
    const unsigned HOLD = 2000000u;   // 2 s in microseconds

    // --- Key table: the 12 Genesis controls on the bits sega_decode emits ---
    assert(PAD_TEST_NUM_KEYS == 12);
    assert(key("Up")->bit    == GP_UP    && !key("Up")->six_only);
    assert(key("Down")->bit  == GP_DOWN  && !key("Down")->six_only);
    assert(key("Left")->bit  == GP_LEFT  && !key("Left")->six_only);
    assert(key("Right")->bit == GP_RIGHT && !key("Right")->six_only);
    assert(key("A")->bit     == GP_A     && !key("A")->six_only);
    assert(key("B")->bit     == GP_B     && !key("B")->six_only);
    assert(key("C")->bit     == GP_RB    && !key("C")->six_only);
    assert(key("Start")->bit == GP_START && !key("Start")->six_only);
    assert(key("X")->bit     == GP_X     && key("X")->six_only);
    assert(key("Y")->bit     == GP_Y     && key("Y")->six_only);
    assert(key("Z")->bit     == GP_LB    && key("Z")->six_only);
    assert(key("Mode")->bit  == GP_SELECT && key("Mode")->six_only);

    // Index constants the screen lays buttons out by.
    assert(strcmp(PAD_TEST_KEYS[PTK_UP].label,    "Up")    == 0);
    assert(strcmp(PAD_TEST_KEYS[PTK_RIGHT].label, "Right") == 0);
    assert(strcmp(PAD_TEST_KEYS[PTK_A].label,     "A")     == 0);
    assert(strcmp(PAD_TEST_KEYS[PTK_C].label,     "C")     == 0);
    assert(strcmp(PAD_TEST_KEYS[PTK_Z].label,     "Z")     == 0);
    assert(strcmp(PAD_TEST_KEYS[PTK_START].label, "Start") == 0);
    assert(strcmp(PAD_TEST_KEYS[PTK_MODE].label,  "Mode")  == 0);

    // --- Per-key state ---
    const PadTestKey &a = *key("A");
    const PadTestKey &x = *key("X");
    assert(pad_test_key_state(a, GP_A, false) == PadKeyState::Pressed);
    assert(pad_test_key_state(a, GP_B, false) == PadKeyState::Released);
    assert(pad_test_key_state(x, GP_X, false) == PadKeyState::Unavailable);  // 3-button pad
    assert(pad_test_key_state(x, 0,    true)  == PadKeyState::Released);
    assert(pad_test_key_state(x, GP_X, true)  == PadKeyState::Pressed);

    // --- Hold Start to exit ---
    bool exit = false;

    // Start still held from opening the screen: ignored until released once.
    HoldExit h;
    hold_exit_begin(h, true);
    assert(hold_exit_update(h, true, 0u,       HOLD, &exit) == 0 && !exit);
    assert(hold_exit_update(h, true, 3000000u, HOLD, &exit) == 0 && !exit);
    assert(hold_exit_update(h, false, 3100000u, HOLD, &exit) == 0 && !exit);

    // Fresh press: half-way at 1 s, a quick release resets the bar.
    assert(hold_exit_update(h, true, 4000000u, HOLD, &exit) == 0   && !exit);
    assert(hold_exit_update(h, true, 5000000u, HOLD, &exit) == 50  && !exit);
    assert(hold_exit_update(h, false, 5100000u, HOLD, &exit) == 0  && !exit);

    // Press again and hold the full 2 s: exit at 100%.
    assert(hold_exit_update(h, true, 6000000u, HOLD, &exit) == 0   && !exit);
    assert(hold_exit_update(h, true, 7999999u, HOLD, &exit) == 99  && !exit);
    assert(hold_exit_update(h, true, 8000000u, HOLD, &exit) == 100 && exit);

    // Not held on entry: armed immediately.
    HoldExit h2;
    hold_exit_begin(h2, false);
    exit = false;
    assert(hold_exit_update(h2, true, 10u,      HOLD, &exit) == 0 && !exit);
    assert(hold_exit_update(h2, true, 2000010u, HOLD, &exit) == 100 && exit);

    // Microsecond clock wraps mid-hold.
    HoldExit h3;
    hold_exit_begin(h3, false);
    exit = false;
    assert(hold_exit_update(h3, true, 0xFFF0BDC0u, HOLD, &exit) == 0 && !exit);  // 2^32 - 1 s
    assert(hold_exit_update(h3, true, 0u,          HOLD, &exit) == 50 && !exit);

    // --- VID:PID label ---
    char vp[10];
    pad_test_vid_pid(vp, sizeof vp, 0x045e, 0x028e);
    assert(strcmp(vp, "045e:028e") == 0);
    char small[5];
    pad_test_vid_pid(small, sizeof small, 0x045e, 0x028e);
    assert(small[sizeof small - 1] == '\0' && strcmp(small, "045e") == 0);

    printf("test_pad_test: OK\n");
    return 0;
}
