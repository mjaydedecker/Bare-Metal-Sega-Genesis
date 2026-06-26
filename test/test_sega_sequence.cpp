#include "../src/input/sega_sequence.h"
#include "../src/input/sega_pad.h"
#include <assert.h>
#include <stdio.h>

// Data-line bit positions (mirror sega_pad.cpp).
enum { D_UP=0, D_DOWN=1, D_LEFT=2, D_RIGHT=3, D_TL=4, D_TR=5 };

// A fake pad: it records every SELECT level written and the number of pin reads,
// and presents a scripted 6-bit data value per phase (present[phase]). read_pin
// derives the phase from the running read count (6 reads per phase).
struct FakePad
{
    int      n_sel;                       // count of set_select() calls
    unsigned sels[SEGA_PHASES + 2];       // recorded SELECT levels
    int      reads;                       // count of read_pin() calls
    uint8_t  present[SEGA_PHASES];        // scripted data per phase (low 6 bits)
};

static void fake_set_select(void *c, unsigned lvl)
{
    FakePad *f = (FakePad *) c;
    if (f->n_sel < (int)(SEGA_PHASES + 2)) f->sels[f->n_sel] = lvl;
    f->n_sel++;
}
static void fake_settle(void *) {}
static unsigned fake_read_pin(void *c, unsigned d)
{
    FakePad *f = (FakePad *) c;
    int phase = f->reads / 6;
    f->reads++;
    if (phase >= SEGA_PHASES) return 1;
    return (f->present[phase] >> d) & 1u;
}

int main(void)
{
    // Script an idle 6-button pad's per-phase data lines.
    FakePad fake;
    fake.n_sel = 0;
    fake.reads = 0;
    fake.present[0] = 0x3F;   // TH=hi : all released
    fake.present[1] = 0x33;   // TH=lo : L/R forced low
    fake.present[2] = 0x3F;
    fake.present[3] = 0x33;
    fake.present[4] = 0x3F;
    fake.present[5] = 0x30;   // 3rd TH=lo : U/D/L/R all low (6-button signature)
    fake.present[6] = 0x3F;   // 4th TH=hi : extras released
    fake.present[7] = 0x33;

    SegaIo io = { fake_set_select, fake_settle, fake_read_pin, &fake };
    SegaSample out[SEGA_PHASES];
    sega_run_sequence(io, out);

    // SELECT was driven hi,lo,hi,lo,... for the 8 phases, then left idle high.
    assert(fake.n_sel == SEGA_PHASES + 1);
    for (int i = 0; i < SEGA_PHASES; ++i)
        assert(fake.sels[i] == (unsigned)((i & 1) ? 0 : 1));
    assert(fake.sels[SEGA_PHASES] == 1);   // idle high after the sequence

    // Exactly 6 pin reads per phase.
    assert(fake.reads == SEGA_PHASES * 6);

    // Each phase's packed data byte matches what the pad presented (bit order).
    for (int i = 0; i < SEGA_PHASES; ++i)
    {
        assert(out[i].sel  == (unsigned)((i & 1) ? 0 : 1));
        assert(out[i].data == fake.present[i]);
    }

    // End-to-end: the assembled samples decode as an idle 6-button pad.
    SegaDecoded d = sega_decode(out);
    assert(d.type == SegaPadType::SixButton);
    assert(d.buttons == 0);

    printf("test_sega_sequence: OK\n");
    return 0;
}
