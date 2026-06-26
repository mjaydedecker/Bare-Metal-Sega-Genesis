# GPIO Real Sega Controllers — Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Read real Sega Genesis 3- and 6-button DB9 controllers wired directly to the Pi GPIO, on two ports, coexisting with the existing USB gamepads.

**Architecture:** A pure, host-tested decoder (`sega_pad`) turns sampled SELECT-phase pin levels into a `GP_*` button bitmask + detected pad type. A thin Circle layer (`gpio_pads`) does the GPIO toggling/sampling and feeds the decoder. A pure `merge_buttons()` ORs the GPIO and USB masks per port at the existing read sites (`input_state_cb`, kernel menu/hotkey reads), so the entire downstream pipeline (joypad_map, menus, hotkeys, MDPAD 3B/6B) is untouched.

**Tech Stack:** C++ (bare-metal, Circle framework, GCC `arm-linux-gnueabihf`), host unit tests in plain C++ with `assert` (system `c++`).

## Global Constraints

- Target: Raspberry Pi 2, AArch32, `RASPPI=2`, `PREFIX=arm-linux-gnueabihf-`. Cross-build = `make` at repo root → `kernel7.img`.
- No bare-metal `snprintf`/heap-heavy idioms in pure modules; pure modules have **no Circle includes** and must compile under the host `c++`.
- Pure decoder modules mirror `src/input/joypad_map.*` (host-tested via `test/Makefile`).
- GP_* bit space is fixed (A,B,X,Y,LB,RB,SELECT,START + UP/DOWN/LEFT/RIGHT) — do not widen it.
- DB9 electrical contract (for the separate HAT project, not this firmware): pin 5 = **+3.3V**, no level shifters/dividers/shift registers. Data inputs use SoC internal pull-ups (`GPIOModeInputPullUp`): **1 = released, 0 = pressed** (active-low).
- GPIO pin map (BCM): see Task 1. Reserved/avoided: 18–21 (I2S), 14/15 (UART), 2/3 (I2C), 0/1 (HAT ID EEPROM).
- Every host test program ends by printing `test_<name>: OK` and returning 0, and is added to both the `run:` list and `clean:` list in `test/Makefile`.
- New compiled source files (`.cpp`) must be added to `OBJS` in the root `Makefile` (header-only modules are not).

---

## File Structure

- `src/input/sega_board.h` / `.cpp` — `BoardPinMap` struct + `kBoardPinMap` constant + `pinmap_unique()`. The one place that knows physical pins. Pure.
- `src/input/sega_pad.h` / `.cpp` — `SegaSample`, `SegaPadType`, `SegaDecoded`, `sega_decode()`. Pure protocol decoder. Includes `joypad_map.h` for GP_* macros only.
- `src/input/input_merge.h` — header-only `merge_buttons(usb, gpio)`. The collision-rule decision point. Pure.
- `src/input/gpio_pads.h` / `.cpp` — `GpioPads` Circle class: owns `CGPIOPin`s, runs the SELECT sequence, decodes, caches per-port mask + type. Not host-tested (build-verified).
- `src/libretro/callbacks.{h,cpp}` — add `g_gpio_pads` global; merge GPIO into `input_state_cb`.
- `src/kernel.cpp` — instantiate/init/poll `GpioPads`; merge into menu/hotkey reads; per-port MDPAD device from live detection; detection toast.
- `Makefile` — add `sega_board.o`, `sega_pad.o`, `gpio_pads.o` to `OBJS`.
- `test/Makefile` — add `test_sega_board`, `test_sega_pad`, `test_input_merge`.
- `docs/hardware-checklist-gpio-controllers.md` — M6 bench-verification checklist.

---

### Task 1: BoardPinMap (M0)

**Files:**
- Create: `src/input/sega_board.h`, `src/input/sega_board.cpp`
- Test: `test/test_sega_board.cpp`
- Modify: `Makefile` (add `src/input/sega_board.o` to `OBJS`), `test/Makefile`

**Interfaces:**
- Produces: `struct PortPins { unsigned select; unsigned data[6]; }`, `struct BoardPinMap { PortPins port[2]; }`, `static const BoardPinMap kBoardPinMap`, `bool pinmap_unique(const BoardPinMap &m)`.

- [ ] **Step 1: Write the failing test**

`test/test_sega_board.cpp`:
```cpp
#include "../src/input/sega_board.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // The shipped pin map has no pin assigned twice.
    assert(pinmap_unique(kBoardPinMap));

    // Sanity: the documented assignment (P1 select=4, P2 select=11).
    assert(kBoardPinMap.port[0].select == 4);
    assert(kBoardPinMap.port[1].select == 11);
    assert(kBoardPinMap.port[0].data[0] == 5);
    assert(kBoardPinMap.port[1].data[5] == 23);

    // A map with a duplicated pin is rejected.
    BoardPinMap bad = kBoardPinMap;
    bad.port[1].data[0] = 4;   // collide with port0 select
    assert(!pinmap_unique(bad));

    printf("test_sega_board: OK\n");
    return 0;
}
```

- [ ] **Step 2: Write `src/input/sega_board.h`**

```cpp
//
// src/input/sega_board.h
//
// Bare Metal Sega Genesis
// Physical GPIO pin assignment for the two DB9 controller ports (Pi 2, BCM
// numbering) — the ONE place that knows physical pins. The separate HAT project
// builds to this table + the 3.3V electrical contract. Data lines D0..D5 map to
// DB9 pins 1,2,3,4,6,9 (Up, Down, Left, Right, TL[B/A], TR[C/Start]).
//

#ifndef _input_sega_board_h
#define _input_sega_board_h

struct PortPins
{
    unsigned select;    // DB9 pin 7 — SELECT (output)
    unsigned data[6];   // D0..D5 (inputs, internal pull-up)
};

struct BoardPinMap
{
    PortPins port[2];
};

// Reserved (do not use): 18-21 (I2S), 14/15 (UART), 2/3 (I2C), 0/1 (HAT EEPROM).
static const BoardPinMap kBoardPinMap =
{
    {
        {  4, {  5,  6,  7,  8,  9, 10 } },   // Port 1 (Player 1)
        { 11, { 12, 13, 16, 17, 22, 23 } },   // Port 2 (Player 2)
    }
};

// True if every GPIO number in the map is distinct.
bool pinmap_unique(const BoardPinMap &m);

#endif
```

- [ ] **Step 3: Write `src/input/sega_board.cpp`**

```cpp
//
// src/input/sega_board.cpp
//
#include "sega_board.h"

bool pinmap_unique(const BoardPinMap &m)
{
    unsigned all[14];
    unsigned n = 0;
    for (int p = 0; p < 2; ++p)
    {
        all[n++] = m.port[p].select;
        for (int d = 0; d < 6; ++d)
            all[n++] = m.port[p].data[d];
    }
    for (unsigned i = 0; i < n; ++i)
        for (unsigned j = i + 1; j < n; ++j)
            if (all[i] == all[j])
                return false;
    return true;
}
```

- [ ] **Step 4: Add the test to `test/Makefile`**

Add `test_sega_board` to the end of the `run:` dependency list and add a `./test_sega_board` line after `./test_controller_map`. Add the build rule:
```make
test_sega_board: test_sega_board.cpp ../src/input/sega_board.cpp ../src/input/sega_board.h
	$(CXX) $(CXXFLAGS) -o $@ test_sega_board.cpp ../src/input/sega_board.cpp
```
Add `test_sega_board` to the `clean:` `rm -f` list.

- [ ] **Step 5: Run the test (expect FAIL, then PASS after build)**

Run: `cd test && make test_sega_board && ./test_sega_board`
Expected: prints `test_sega_board: OK`.

- [ ] **Step 6: Add to root `OBJS` and cross-build**

In `Makefile`, add `src/input/sega_board.o \` to the `OBJS` list (e.g. after `src/input/hotkey.o`).
Run: `make` (repo root)
Expected: builds `kernel7.img` with no errors.

- [ ] **Step 7: Commit**

```bash
git add src/input/sega_board.h src/input/sega_board.cpp test/test_sega_board.cpp test/Makefile Makefile
git commit -m "feat(input): GPIO controller board pin map (M0)"
```

---

### Task 2: sega_pad decoder — presence, 3-button, d-pad/A/B/C/Start (M1a)

**Files:**
- Create: `src/input/sega_pad.h`, `src/input/sega_pad.cpp`
- Test: `test/test_sega_pad.cpp`
- Modify: `Makefile` (`OBJS`), `test/Makefile`

**Interfaces:**
- Consumes: GP_* macros from `src/input/joypad_map.h`.
- Produces:
  - `#define SEGA_PHASES 8`
  - `struct SegaSample { uint8_t sel; uint8_t data; }` (`data` low 6 bits = D0..D5 as read, 1=released/0=pressed)
  - `enum class SegaPadType : uint8_t { None, Unsupported, ThreeButton, SixButton }`
  - `struct SegaDecoded { SegaPadType type; unsigned buttons; }`
  - `SegaDecoded sega_decode(const SegaSample phases[SEGA_PHASES])`

- [ ] **Step 1: Write `src/input/sega_pad.h`**

```cpp
//
// src/input/sega_pad.h
//
// Bare Metal Sega Genesis
// Pure SELECT-multiplex decoder for Sega DB9 pads. A full poll captures
// SEGA_PHASES alternating-SELECT samples (hi,lo,hi,lo,...) — enough to cover the
// 6-button sequence. Data bits are stored AS READ (pull-ups: 1=released,
// 0=pressed); the decoder outputs an active-high GP_* bitmask. No Circle deps.
//

#ifndef _input_sega_pad_h
#define _input_sega_pad_h

#include <stdint.h>
#include "joypad_map.h"   // GP_* bit constants (macros only)

#define SEGA_PHASES 8

struct SegaSample
{
    uint8_t sel;    // 1 = SELECT high, 0 = SELECT low (as driven)
    uint8_t data;   // low 6 bits = D0..D5 as read (1=released, 0=pressed)
};

enum class SegaPadType : uint8_t
{
    None,          // no pad / no response (all lines released every phase)
    Unsupported,   // responds but not a Genesis pad (no L/R-forced-low signature)
    ThreeButton,
    SixButton,
};

struct SegaDecoded
{
    SegaPadType type;
    unsigned    buttons;   // GP_* bitmask, 1 = pressed
};

// Decode one full 8-phase poll into a pad type + GP_* bitmask.
SegaDecoded sega_decode(const SegaSample phases[SEGA_PHASES]);

#endif
```

- [ ] **Step 2: Write the failing test (presence + 3-button)**

`test/test_sega_pad.cpp`:
```cpp
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

    printf("test_sega_pad: OK\n");
    return 0;
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd test && make test_sega_pad`
Expected: FAIL — link error, `sega_decode` undefined.

(Add the build rule first if `make` reports no rule — see Step 5.)

- [ ] **Step 4: Write `src/input/sega_pad.cpp` (presence + 3-button only)**

```cpp
//
// src/input/sega_pad.cpp
//
#include "sega_pad.h"

enum { D_UP = 0, D_DOWN = 1, D_LEFT = 2, D_RIGHT = 3, D_TL = 4, D_TR = 5 };

static inline bool pressed(uint8_t data, int bit)
{
    return ((data >> bit) & 1u) == 0u;   // active-low: 0 means pressed
}

SegaDecoded sega_decode(const SegaSample phases[SEGA_PHASES])
{
    SegaDecoded out;
    out.type = SegaPadType::None;
    out.buttons = 0;

    // Presence: any line low on any phase = something responded.
    bool anyResponse = false;
    for (int i = 0; i < SEGA_PHASES; ++i)
        if ((phases[i].data & 0x3F) != 0x3F) { anyResponse = true; break; }
    if (!anyResponse)
        return out;   // None

    // Genesis signature: at a TH=low read, Left+Right are forced low.
    const SegaSample &lo = phases[1];
    bool genesis = pressed(lo.data, D_LEFT) && pressed(lo.data, D_RIGHT);
    if (!genesis) { out.type = SegaPadType::Unsupported; return out; }

    // D-pad + B/C from the TH=high read.
    const SegaSample &hi = phases[0];
    if (pressed(hi.data, D_UP))    out.buttons |= GP_UP;
    if (pressed(hi.data, D_DOWN))  out.buttons |= GP_DOWN;
    if (pressed(hi.data, D_LEFT))  out.buttons |= GP_LEFT;
    if (pressed(hi.data, D_RIGHT)) out.buttons |= GP_RIGHT;
    if (pressed(hi.data, D_TL))    out.buttons |= GP_B;    // B
    if (pressed(hi.data, D_TR))    out.buttons |= GP_RB;   // C

    // A + Start from the TH=low read (L/R are forced low here, ignore them).
    if (pressed(lo.data, D_TL))    out.buttons |= GP_A;     // A
    if (pressed(lo.data, D_TR))    out.buttons |= GP_START; // Start

    out.type = SegaPadType::ThreeButton;   // 6-button upgrade added in Task 3
    return out;
}
```

- [ ] **Step 5: Add the test build rule and run**

In `test/Makefile` add `test_sega_pad` to `run:` deps + a `./test_sega_pad` line, the rule (note `-I$(LIBRETRO_INC)` because `sega_pad.h` includes `joypad_map.h`):
```make
test_sega_pad: test_sega_pad.cpp ../src/input/sega_pad.cpp ../src/input/sega_pad.h ../src/input/joypad_map.h
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_sega_pad.cpp ../src/input/sega_pad.cpp
```
Add `test_sega_pad` to `clean:`.
Run: `cd test && make test_sega_pad && ./test_sega_pad`
Expected: `test_sega_pad: OK`.

- [ ] **Step 6: Commit**

```bash
git add src/input/sega_pad.h src/input/sega_pad.cpp test/test_sega_pad.cpp test/Makefile
git commit -m "feat(input): pure Sega pad decoder — presence + 3-button (M1a)"
```

---

### Task 3: sega_pad decoder — 6-button detection + extras (M1b)

**Files:**
- Modify: `src/input/sega_pad.cpp`
- Modify: `test/test_sega_pad.cpp`
- Modify: `Makefile` (add `src/input/sega_pad.o` to `OBJS`)

**Interfaces:**
- Consumes/Produces: same `sega_decode` signature; now also returns `SegaPadType::SixButton` and sets GP_LB (Z), GP_Y (Y), GP_X (X), GP_SELECT (Mode).

- [ ] **Step 1: Add the failing 6-button test (append inside `main`, before the final printf)**

In `test/test_sega_pad.cpp`, before `printf("test_sega_pad: OK\n");`:
```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test && make test_sega_pad && ./test_sega_pad`
Expected: FAIL — type is `ThreeButton`, assertion `d6.type == SixButton` fails.

- [ ] **Step 3: Implement 6-button in `src/input/sega_pad.cpp`**

Replace the final two lines of `sega_decode` (`out.type = SegaPadType::ThreeButton; return out;`) with:
```cpp
    // 6-button signature: the 3rd TH=low read (phase 5) forces U/D/L/R all low.
    const SegaSample &sig = phases[5];
    bool six = pressed(sig.data, D_UP) && pressed(sig.data, D_DOWN)
            && pressed(sig.data, D_LEFT) && pressed(sig.data, D_RIGHT);
    if (six)
    {
        out.type = SegaPadType::SixButton;
        const SegaSample &ex = phases[6];   // 4th TH=high: Z,Y,X,Mode
        if (pressed(ex.data, D_UP))    out.buttons |= GP_LB;     // Z
        if (pressed(ex.data, D_DOWN))  out.buttons |= GP_Y;      // Y
        if (pressed(ex.data, D_LEFT))  out.buttons |= GP_X;      // X
        if (pressed(ex.data, D_RIGHT)) out.buttons |= GP_SELECT; // Mode
    }
    else
    {
        out.type = SegaPadType::ThreeButton;
    }
    return out;
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd test && make test_sega_pad && ./test_sega_pad`
Expected: `test_sega_pad: OK`.

- [ ] **Step 5: Add to root `OBJS` and cross-build**

In `Makefile`, add `src/input/sega_pad.o \` to `OBJS`.
Run: `make`
Expected: builds `kernel7.img` cleanly.

- [ ] **Step 6: Commit**

```bash
git add src/input/sega_pad.cpp test/test_sega_pad.cpp Makefile
git commit -m "feat(input): Sega pad decoder — 6-button detection + extras (M1b)"
```

---

### Task 4: input_merge collision rule (M3a)

**Files:**
- Create: `src/input/input_merge.h` (header-only)
- Test: `test/test_input_merge.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Produces: `unsigned merge_buttons(unsigned usb, unsigned gpio)` (inline).

- [ ] **Step 1: Write the failing test**

`test/test_input_merge.cpp`:
```cpp
#include "../src/input/input_merge.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(merge_buttons(0, 0) == 0);
    assert(merge_buttons(0x1, 0x0) == 0x1);   // USB only
    assert(merge_buttons(0x0, 0x2) == 0x2);   // GPIO only
    assert(merge_buttons(0x1, 0x2) == 0x3);   // both sources OR together
    assert(merge_buttons(0x5, 0x4) == 0x5);   // overlap is idempotent
    printf("test_input_merge: OK\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test && make test_input_merge`
Expected: FAIL — no rule / `input_merge.h` not found.

- [ ] **Step 3: Write `src/input/input_merge.h`**

```cpp
//
// src/input/input_merge.h
//
// Bare Metal Sega Genesis
// Collision rule for coexisting input sources (USB + GPIO) on one port.
// Decision (spec M3): OR both sources. A port rarely has both a USB and a DB9
// pad at once, so OR is identical in practice while avoiding a presence-priority
// edge case. This is the single place that rule lives. Pure, no Circle deps.
//

#ifndef _input_input_merge_h
#define _input_input_merge_h

static inline unsigned merge_buttons(unsigned usb, unsigned gpio)
{
    return usb | gpio;
}

#endif
```

- [ ] **Step 4: Add the test rule and run**

In `test/Makefile` add `test_input_merge` to `run:` deps + `./test_input_merge` line + `clean:`. Rule:
```make
test_input_merge: test_input_merge.cpp ../src/input/input_merge.h
	$(CXX) $(CXXFLAGS) -o $@ test_input_merge.cpp
```
Run: `cd test && make test_input_merge && ./test_input_merge`
Expected: `test_input_merge: OK`.

- [ ] **Step 5: Commit**

```bash
git add src/input/input_merge.h test/test_input_merge.cpp test/Makefile
git commit -m "feat(input): merge_buttons collision rule for USB+GPIO coexist (M3a)"
```

---

### Task 5: GpioPads Circle reader (M2)

**Files:**
- Create: `src/input/gpio_pads.h`, `src/input/gpio_pads.cpp`
- Modify: `Makefile` (`OBJS`)

**Interfaces:**
- Consumes: `kBoardPinMap` (Task 1), `sega_decode`/`SegaPadType`/`SegaSample`/`SEGA_PHASES` (Tasks 2-3), Circle `CGPIOPin`, `CTimer`.
- Produces:
  - `class GpioPads` with `static const unsigned NUM_PORTS = 2;`
  - `void Init(void)`, `void Poll(void)`
  - `unsigned Buttons(unsigned port) const` (GP_* mask; 0 if invalid)
  - `boolean IsPresent(unsigned port) const`
  - `SegaPadType PadTypeAt(unsigned port) const`

> No host test — Circle-dependent. Verification = cross-build (`make`). Behavior is exercised on hardware in Task 8 / the M6 checklist.

- [ ] **Step 1: Write `src/input/gpio_pads.h`**

```cpp
//
// src/input/gpio_pads.h
//
// Bare Metal Sega Genesis
// Reads real Sega DB9 controllers wired directly to GPIO (per sega_board.h).
// Drives SELECT and samples the 6 data lines across SEGA_PHASES, feeding the
// pure sega_decode(). Mirrors Gamepad's shape so it can be a second source of
// the per-port GP_* bitmask. Pads are powered at 3.3V (see HAT electrical
// contract); data inputs use internal pull-ups (1=released, 0=pressed).
//

#ifndef _input_gpio_pads_h
#define _input_gpio_pads_h

#include <circle/gpiopin.h>
#include <circle/types.h>
#include "sega_pad.h"
#include "sega_board.h"

class GpioPads
{
public:
    static const unsigned NUM_PORTS = 2;

    GpioPads(void);

    // Configure the GPIO pin modes from kBoardPinMap. Call once after boot.
    void Init(void);

    // Run the SELECT sequence for both ports, decode, and cache. Call per frame.
    void Poll(void);

    unsigned    Buttons(unsigned port) const;     // GP_* mask (0 if invalid)
    boolean     IsPresent(unsigned port) const;
    SegaPadType PadTypeAt(unsigned port) const;

private:
    void PollPort(unsigned port, SegaSample out[SEGA_PHASES]);

    CGPIOPin    m_Select[NUM_PORTS];
    CGPIOPin    m_Data[NUM_PORTS][6];
    unsigned    m_Buttons[NUM_PORTS];
    SegaPadType m_Type[NUM_PORTS];
};

#endif
```

- [ ] **Step 2: Write `src/input/gpio_pads.cpp`**

```cpp
//
// src/input/gpio_pads.cpp
//
#include "gpio_pads.h"
#include <circle/timer.h>

// Settle time after toggling SELECT before sampling. The pad's HC mux switches
// in tens of ns; a few µs is generous and keeps a full 8-phase poll well under
// 100 µs — negligible against the ~16.67 ms frame.
#define SELECT_SETTLE_US 5

GpioPads::GpioPads(void)
{
    for (unsigned p = 0; p < NUM_PORTS; ++p)
    {
        m_Buttons[p] = 0;
        m_Type[p] = SegaPadType::None;
    }
}

void GpioPads::Init(void)
{
    for (unsigned p = 0; p < NUM_PORTS; ++p)
    {
        m_Select[p].AssignPin(kBoardPinMap.port[p].select);
        m_Select[p].SetMode(GPIOModeOutput);
        m_Select[p].Write(1);   // idle SELECT high
        for (unsigned d = 0; d < 6; ++d)
        {
            m_Data[p][d].AssignPin(kBoardPinMap.port[p].data[d]);
            m_Data[p][d].SetMode(GPIOModeInputPullUp);
        }
    }
}

void GpioPads::PollPort(unsigned port, SegaSample out[SEGA_PHASES])
{
    for (unsigned i = 0; i < SEGA_PHASES; ++i)
    {
        unsigned sel = (i & 1) ? 0 : 1;          // hi, lo, hi, lo, ...
        m_Select[port].Write(sel);
        CTimer::SimpleusDelay(SELECT_SETTLE_US);
        u8 data = 0;
        for (unsigned d = 0; d < 6; ++d)
            data |= (u8)((m_Data[port][d].Read() & 1u) << d);
        out[i].sel = (u8) sel;
        out[i].data = data;
    }
    m_Select[port].Write(1);   // leave SELECT idle high (lets the 6-btn counter reset)
}

void GpioPads::Poll(void)
{
    for (unsigned p = 0; p < NUM_PORTS; ++p)
    {
        SegaSample phases[SEGA_PHASES];
        PollPort(p, phases);
        SegaDecoded d = sega_decode(phases);
        m_Type[p] = d.type;
        m_Buttons[p] =
            (d.type == SegaPadType::ThreeButton || d.type == SegaPadType::SixButton)
            ? d.buttons : 0;
    }
}

unsigned GpioPads::Buttons(unsigned port) const
{
    return (port < NUM_PORTS) ? m_Buttons[port] : 0;
}

boolean GpioPads::IsPresent(unsigned port) const
{
    if (port >= NUM_PORTS) return FALSE;
    return (m_Type[port] == SegaPadType::ThreeButton
         || m_Type[port] == SegaPadType::SixButton) ? TRUE : FALSE;
}

SegaPadType GpioPads::PadTypeAt(unsigned port) const
{
    return (port < NUM_PORTS) ? m_Type[port] : SegaPadType::None;
}
```

- [ ] **Step 3: Add to root `OBJS` and cross-build**

In `Makefile`, add `src/input/gpio_pads.o \` to `OBJS`.
Run: `make`
Expected: builds `kernel7.img` cleanly (compiles against Circle `CGPIOPin`/`CTimer`).

- [ ] **Step 4: Commit**

```bash
git add src/input/gpio_pads.h src/input/gpio_pads.cpp Makefile
git commit -m "feat(input): GpioPads Circle reader — SELECT sequence + decode (M2)"
```

---

### Task 6: Coexist integration — callbacks + kernel poll/menu/hotkey (M3b)

**Files:**
- Modify: `src/libretro/callbacks.h`, `src/libretro/callbacks.cpp`
- Modify: `src/kernel.cpp`

**Interfaces:**
- Consumes: `GpioPads` (Task 5), `merge_buttons` (Task 4).
- Produces: `extern GpioPads *g_gpio_pads;` (definition in callbacks.cpp); kernel member `GpioPads m_GpioPads;` polled each frame; `unsigned CKernel::MergedMenuButtons(void)`.

- [ ] **Step 1: Declare the global in `src/libretro/callbacks.h`**

Add near the existing `extern Gamepad *g_gamepad;` declaration:
```cpp
class GpioPads;            // forward declaration
extern GpioPads *g_gpio_pads;   // optional second input source (0 if none)
```

- [ ] **Step 2: Merge GPIO into `input_state_cb` (`src/libretro/callbacks.cpp`)**

Add includes at the top with the other input includes:
```cpp
#include "../input/gpio_pads.h"
#include "../input/input_merge.h"
```
Add the global definition next to `Gamepad *g_gamepad = 0;`:
```cpp
GpioPads *g_gpio_pads = 0;
```
Change the button read (currently `unsigned buttons = g_gamepad->Buttons(port);`) to merge GPIO first, before the hotkey-mask check:
```cpp
    unsigned buttons = g_gamepad->Buttons(port);
    if (g_gpio_pads)
        buttons = merge_buttons(buttons, g_gpio_pads->Buttons(port));
```

- [ ] **Step 3: Add the kernel member + init (`src/kernel.cpp`)**

Add the include with the other input includes:
```cpp
#include "input/gpio_pads.h"
#include "input/input_merge.h"
```
Add a member alongside `m_Gamepad` in the class declaration (find the `Gamepad m_Gamepad;` member in `src/kernel.h` and add after it):
```cpp
	GpioPads m_GpioPads;
```
> NOTE: if `m_Gamepad` is declared in `src/kernel.h`, add `m_GpioPads` there and (optionally) to the constructor init list in `src/kernel.cpp`. `GpioPads` has a default constructor, so no init-list entry is required.

In `CKernel::Initialize` (near `g_gamepad = &m_Gamepad;`, after USB is up) add:
```cpp
	m_GpioPads.Init ();
	g_gpio_pads = &m_GpioPads;
```

- [ ] **Step 4: Add the merged-menu-buttons helper**

Declare in `src/kernel.h` (private section):
```cpp
	unsigned MergedMenuButtons (void);
```
Define in `src/kernel.cpp`:
```cpp
unsigned CKernel::MergedMenuButtons (void)
{
	return m_Gamepad.MenuButtons ()
	     | m_GpioPads.Buttons (0)
	     | m_GpioPads.Buttons (1);
}
```

- [ ] **Step 5: Poll GPIO + use merged reads in the play loop (`src/kernel.cpp`)**

After `m_Gamepad.Poll ();` (around line 308) add:
```cpp
			m_GpioPads.Poll ();
```
Replace the two `m_Gamepad.MenuButtons ()` reads (the `unsigned now = ...` around line 309 and the `prevBtns = ...` resync around line 326) with `MergedMenuButtons ()`.
Replace the player-1 read (`unsigned p1now = m_Gamepad.Buttons (0);` around line 334) with:
```cpp
			unsigned     p1now     = merge_buttons (m_Gamepad.Buttons (0),
			                                        m_GpioPads.Buttons (0));
```

- [ ] **Step 6: Cross-build**

Run: `make`
Expected: builds `kernel7.img` cleanly. (No host test — this is integration; the pure pieces it wires were tested in Tasks 2-4.)

- [ ] **Step 7: Run the full host test suite (regression)**

Run: `cd test && make run`
Expected: every `test_*: OK` line prints, including the three new ones; no assertion aborts.

- [ ] **Step 8: Commit**

```bash
git add src/libretro/callbacks.h src/libretro/callbacks.cpp src/kernel.cpp src/kernel.h
git commit -m "feat(input): wire GPIO pads into input_state_cb + menu/hotkey reads (M3b)"
```

---

### Task 7: Per-port live pad-type → MDPAD device (M4)

**Files:**
- Modify: `src/kernel.cpp`, `src/kernel.h`

**Interfaces:**
- Consumes: `m_GpioPads.PadTypeAt(port)`, `m_Settings.pad_type`, `RETRO_DEVICE_MDPAD_3B/6B` (already defined around kernel.cpp:202).
- Produces: `unsigned CKernel::PadDeviceFor(unsigned port)`.

- [ ] **Step 1: Add the per-port device selector**

Declare in `src/kernel.h` (private):
```cpp
	unsigned PadDeviceFor (unsigned port);
```
Define in `src/kernel.cpp` (place it near the other CKernel methods, after `MergedMenuButtons`). It assumes the `RETRO_DEVICE_MDPAD_*` macros at kernel.cpp:202 are in scope; if they are local `#define`s inside the run method, move those two `#define`s to file scope (top of kernel.cpp, after the includes) so both sites share them:
```cpp
unsigned CKernel::PadDeviceFor (unsigned port)
{
	SegaPadType t = m_GpioPads.PadTypeAt (port);
	if (t == SegaPadType::SixButton)   return RETRO_DEVICE_MDPAD_6B;
	if (t == SegaPadType::ThreeButton) return RETRO_DEVICE_MDPAD_3B;
	// No GPIO pad on this port: honor the global setting (USB pads use this).
	return (m_Settings.pad_type == PadType::ThreeButton)
	     ? RETRO_DEVICE_MDPAD_3B : RETRO_DEVICE_MDPAD_6B;
}
```

- [ ] **Step 2: Use it at ROM load**

Replace the block at `src/kernel.cpp:284-287`:
```cpp
		unsigned padDev = (m_Settings.pad_type == PadType::ThreeButton)
		                  ? RETRO_DEVICE_MDPAD_3B : RETRO_DEVICE_MDPAD_6B;
		retro_set_controller_port_device (0, padDev);
		retro_set_controller_port_device (1, padDev);
```
with:
```cpp
		m_GpioPads.Poll ();   // get a fresh pad-type read before choosing devices
		retro_set_controller_port_device (0, PadDeviceFor (0));
		retro_set_controller_port_device (1, PadDeviceFor (1));
```

- [ ] **Step 3: Cross-build**

Run: `make`
Expected: builds `kernel7.img` cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/kernel.cpp src/kernel.h
git commit -m "feat(input): per-port MDPAD 3B/6B from live GPIO detection (M4)"
```

---

### Task 8: Detection indicator (M5) + hardware-verify checklist (M6)

**Files:**
- Modify: `src/kernel.cpp`
- Create: `docs/hardware-checklist-gpio-controllers.md`

**Interfaces:**
- Consumes: `m_GpioPads.IsPresent/PadTypeAt`, `m_Overlay.ShowToast` (existing; see kernel.cpp:343).

- [ ] **Step 1: Show a detection toast at ROM load**

In `src/kernel.cpp`, right after the `retro_set_controller_port_device` calls added in Task 7, add:
```cpp
		for (unsigned p = 0; p < GpioPads::NUM_PORTS; ++p)
		{
			if (!m_GpioPads.IsPresent (p))
				continue;
			// "GPIO P0: N-button" — the '0' at index 6 is the port placeholder.
			const char *kind =
				(m_GpioPads.PadTypeAt (p) == SegaPadType::SixButton)
				? "GPIO P0: 6-button" : "GPIO P0: 3-button";
			char msg[20];
			unsigned i = 0;
			for (; kind[i] != '\0' && i < sizeof msg - 1; ++i)
				msg[i] = kind[i];
			msg[i] = '\0';
			msg[6] = (char)('1' + p);   // patch port digit: P0 -> P1 / P2
			m_Overlay.ShowToast (msg, TOAST_SUCCESS);
		}
```
> NOTE: no `snprintf` on bare metal — the bounded copy-and-patch above is allocation-free and stops at the literal's NUL. `ShowToast(const char*, ToastKind)` matches the existing call at kernel.cpp:343.

- [ ] **Step 2: Cross-build**

Run: `make`
Expected: builds `kernel7.img` cleanly.

- [ ] **Step 3: Write the M6 hardware checklist**

`docs/hardware-checklist-gpio-controllers.md`:
```markdown
# Hardware Checklist — GPIO Sega Controllers (checklist W)

Bench setup: DB9 jacks wired to GPIO per `src/input/sega_board.h` (jumper harness
or HAT). **DB9 pin 5 = +3.3V (NOT 5V)**, pin 8 = GND. Verify pin 5 with a
multimeter before connecting a pad.

- [ ] W1. Boot with no GPIO pad connected: USB pads still work; no phantom input.
- [ ] W2. 3-button pad on Port 1: d-pad + A/B/C/Start all register; detection
      toast shows "GPIO pad P1 : 3-button".
- [ ] W3. 6-button pad on Port 1: A/B/C + X/Y/Z + Mode + Start all register;
      toast shows "6-button"; core runs in 6-button mode.
- [ ] W4. Port 2 pad: independent of Port 1, both players controllable.
- [ ] W5. Coexist: a USB pad on P1 and a DB9 pad on P2 both drive their players;
      either opens the pause menu (MergedMenuButtons).
- [ ] W6. In-game hotkeys (Select+button) work from the DB9 pad on P1.
- [ ] W7. Latency/perf: 60 fps maintained, no audio underruns vs USB-only.
- [ ] W8. Hot-swap: unplug/replug a DB9 pad — presence re-detects within a frame
      (polled each Poll, no hotplug event needed).
```

- [ ] **Step 4: Run the full host test suite (final regression)**

Run: `cd test && make run`
Expected: all `test_*: OK`, no failures.

- [ ] **Step 5: Commit**

```bash
git add src/kernel.cpp docs/hardware-checklist-gpio-controllers.md
git commit -m "feat(input): GPIO pad detection toast + M6 hardware checklist (M5/M6)"
```

---

## Notes for the implementer

- **Detection edge cases are intentional.** The 6-button signature (U/D/L/R all low at phase 5) can't be forged by a 3-button pad because a d-pad can't press Up+Down together. The Atari/SMS "Unsupported" path simply yields no buttons — safe.
- **Genesis→GP_* mapping** lives entirely in `sega_decode` (Task 2/3): A→GP_A, B→GP_B, C→GP_RB, X→GP_X, Y→GP_Y, Z→GP_LB, Start→GP_START, Mode→GP_SELECT. If hardware testing (W3) shows a button feels wrong in-game, retune that single table — downstream `joypad_map` is unchanged.
- **Timing safety:** `Poll()` leaves SELECT idle-high; the ~16 ms gap between frame polls is far longer than the 6-button counter's ~1.5 ms reset window, so each poll starts clean. Do not call `Poll()` twice within 1.5 ms.
- The two spec "open decisions" are resolved here: **collision rule = OR** (Task 4) and **disable-toggle = omitted (YAGNI)** — M5 ships only the detection indicator.
```

