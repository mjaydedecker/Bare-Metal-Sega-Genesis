#include "../src/input/sega_pad.h"
#include <assert.h>
#include <stdio.h>

// Data-line bit positions (mirror sega_pad.cpp).
enum { D_UP=0, D_DOWN=1, D_LEFT=2, D_RIGHT=3, D_TL=4, D_TR=5 };
static const uint8_t REL = 0x3F;   // all six lines released

// Build an 8-phase array with every line released, SELECT alternating hi,lo.
static void idle(SegaSample p[SEGA_PHASES])
{
    for (int i = 0; i < SEGA_PHASES; ++i) { p[i].sel = (i & 1) ? 0 : 1; p[i].data = REL; }
}
static inline void press(SegaSample &s, int bit) { s.data &= (uint8_t)~(1u << bit); }

int main(void)
{
    // No pad: every line released on every phase -> None.
    SegaSample none[SEGA_PHASES]; idle(none);
    assert(sega_decode(none).type == SegaPadType::None);

    // A 3-button pad: at every TH=low phase L/R are forced low. Up/Down reflect
    // actual state, so the 6-button signature (U/D/L/R all low at phase 5) fails.
    SegaSample three[SEGA_PHASES]; idle(three);
    for (int i = 1; i < SEGA_PHASES; i += 2) { press(three[i], D_LEFT); press(three[i], D_RIGHT); }
    SegaDecoded d = sega_decode(three);
    assert(d.type == SegaPadType::ThreeButton);
    assert(d.buttons == 0);

    // Press Start (TR at TH=low) and A (TL at TH=low) and Right (TH=high).
    press(three[1], D_TR);    // Start
    press(three[1], D_TL);    // A
    press(three[0], D_RIGHT); // Right (read at TH=high phase 0)
    // re-stamp every low phase with the forced L/R + Start/A for realism
    for (int i = 3; i < SEGA_PHASES; i += 2) { press(three[i], D_LEFT); press(three[i], D_RIGHT); }
    d = sega_decode(three);
    assert(d.type == SegaPadType::ThreeButton);
    assert(d.buttons & GP_START);
    assert(d.buttons & GP_A);
    assert(d.buttons & GP_RIGHT);
    assert(!(d.buttons & GP_B));

    // An Atari/SMS pad: responds (some line low) but L/R NOT forced low at TH=low.
    SegaSample atari[SEGA_PHASES]; idle(atari);
    press(atari[0], D_TL);    // some activity, but no L/R-forced-low signature
    assert(sega_decode(atari).type == SegaPadType::Unsupported);

    // 6-button pad: phase 5 (3rd TH=low) forces U/D/L/R all low = signature;
    // phase 6 (4th TH=high) exposes Z,Y,X,Mode on the U/D/L/R lines.
    SegaSample six[SEGA_PHASES]; idle(six);
    for (int i = 1; i < SEGA_PHASES; i += 2) { press(six[i], D_LEFT); press(six[i], D_RIGHT); }
    press(six[5], D_UP); press(six[5], D_DOWN);   // complete the all-low signature
    SegaDecoded d6 = sega_decode(six);
    assert(d6.type == SegaPadType::SixButton);
    assert(d6.buttons == 0);

    // Press X (D_LEFT) and Mode (D_RIGHT) at the extras phase (6).
    press(six[6], D_LEFT);    // X
    press(six[6], D_RIGHT);   // Mode
    d6 = sega_decode(six);
    assert(d6.type == SegaPadType::SixButton);
    assert(d6.buttons & GP_X);
    assert(d6.buttons & GP_SELECT);   // Mode
    assert(!(d6.buttons & GP_LB));    // Z not pressed

    printf("test_sega_pad: OK\n");
    return 0;
}
