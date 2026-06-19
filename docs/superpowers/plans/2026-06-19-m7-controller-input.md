# M7 Controller Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A USB gamepad controls the emulated Genesis (D-pad + 6 buttons + Start + Mode).

**Architecture:** A `Gamepad` class wraps Circle's generic `CUSBGamePadDevice` (found via the device-name service, polled per frame for its normalized button bitmask). A pure, host-tested `joypad_state()` maps libretro joypad ids → gamepad button bits by role. The input callbacks reach the pad via a `g_gamepad` global; the kernel sets port 0 to a 6-button MD pad with `retro_set_controller_port_device`.

**Tech Stack:** C++, bare-metal Circle (Pi 2 / AArch32), Genesis-Plus-GX-Wide. Pure mapping logic is host-tested.

**Reference spec:** `docs/superpowers/specs/2026-06-19-m7-controller-input-design.md`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `src/input/joypad_map.h` / `.cpp` (new) | Pure `joypad_state(buttons, retro_id)` — no Circle deps; host-testable. |
| `src/input/gamepad.h` / `.cpp` (new) | `Gamepad` — find/poll `CUSBGamePadDevice`; `static_assert` button-bit mirror. |
| `src/libretro/callbacks.{h,cpp}` (modify) | Implement `input_poll_cb`/`input_state_cb`; add `g_gamepad`. |
| `src/kernel.{h,cpp}` (modify) | `m_Gamepad` member, init, 6-button port setup, set `g_gamepad`. |
| `Makefile` (modify) | Add `src/input/*.o` to `OBJS`/`EXTRACLEAN`. |
| `test/test_joypad.cpp`, `test/Makefile` (new/modify) | Host tests for `joypad_state`. |

---

## Task 1: Pure `joypad_state` mapping (host-tested, TDD)

**Files:**
- Create: `src/input/joypad_map.h`
- Create: `src/input/joypad_map.cpp`
- Create: `test/test_joypad.cpp`
- Modify: `test/Makefile`

- [ ] **Step 1: Write the header**

Create `src/input/joypad_map.h`:

```cpp
//
// src/input/joypad_map.h
//
// Bare Metal Sega Genesis
// Pure libretro-joypad-id -> gamepad-button mapping. No Circle deps so it is
// host-testable. The GP_* bits mirror Circle's TGamePadButton (verified by a
// static_assert in gamepad.cpp).
//

#ifndef _input_joypad_map_h
#define _input_joypad_map_h

#include <stdint.h>
#include <libretro.h>   // RETRO_DEVICE_ID_JOYPAD_*

#define GP_Y      (1u << 7)
#define GP_B      (1u << 8)
#define GP_A      (1u << 9)
#define GP_X      (1u << 10)
#define GP_LB     (1u << 5)
#define GP_RB     (1u << 6)
#define GP_SELECT (1u << 11)
#define GP_START  (1u << 14)
#define GP_UP     (1u << 15)
#define GP_RIGHT  (1u << 16)
#define GP_DOWN   (1u << 17)
#define GP_LEFT   (1u << 18)

// buttons: TGamePadButton bitmask. retro_id: RETRO_DEVICE_ID_JOYPAD_*.
// Returns 1 if the mapped button is pressed, else 0.
int16_t joypad_state(unsigned buttons, unsigned retro_id);

#endif
```

- [ ] **Step 2: Write the failing test**

Create `test/test_joypad.cpp`:

```cpp
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

    printf("All joypad tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add a test target**

In `test/Makefile`, change the `run` target to build/run both suites and add the `test_joypad` rule (it needs the libretro-common include path for `libretro.h`):

```makefile
# Host-side unit tests (system compiler, not the cross toolchain).
CXX      ?= c++
CXXFLAGS ?= -Wall -Wextra -O2

LIBRETRO_INC = ../libs/genesis-plus-gx-wide/libretro/libretro-common/include

.PHONY: run clean

run: test_blit test_joypad
	./test_blit
	./test_joypad

test_blit: test_blit.cpp ../src/video/blit.cpp ../src/video/blit.h
	$(CXX) $(CXXFLAGS) -o $@ test_blit.cpp ../src/video/blit.cpp

test_joypad: test_joypad.cpp ../src/input/joypad_map.cpp ../src/input/joypad_map.h
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_joypad.cpp ../src/input/joypad_map.cpp

clean:
	rm -f test_blit test_joypad
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test run`
Expected: `test_joypad` fails to build — `undefined reference to 'joypad_state'` (no implementation yet). (`test_blit` still passes.)

- [ ] **Step 5: Implement `joypad_state`**

Create `src/input/joypad_map.cpp`:

```cpp
//
// src/input/joypad_map.cpp
//
// Bare Metal Sega Genesis
// See joypad_map.h.
//

#include "joypad_map.h"

int16_t joypad_state(unsigned buttons, unsigned retro_id)
{
    unsigned mask;
    switch (retro_id)
    {
    case RETRO_DEVICE_ID_JOYPAD_UP:     mask = GP_UP;     break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:   mask = GP_DOWN;   break;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:   mask = GP_LEFT;   break;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT:  mask = GP_RIGHT;  break;
    case RETRO_DEVICE_ID_JOYPAD_A:      mask = GP_A;      break;
    case RETRO_DEVICE_ID_JOYPAD_B:      mask = GP_B;      break;
    case RETRO_DEVICE_ID_JOYPAD_X:      mask = GP_X;      break;
    case RETRO_DEVICE_ID_JOYPAD_Y:      mask = GP_Y;      break;
    case RETRO_DEVICE_ID_JOYPAD_L:      mask = GP_LB;     break;
    case RETRO_DEVICE_ID_JOYPAD_R:      mask = GP_RB;     break;
    case RETRO_DEVICE_ID_JOYPAD_START:  mask = GP_START;  break;
    case RETRO_DEVICE_ID_JOYPAD_SELECT: mask = GP_SELECT; break;
    default:                            return 0;
    }
    return (buttons & mask) ? 1 : 0;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test run`
Expected: `All blit tests passed` and `All joypad tests passed`, exit 0.

- [ ] **Step 7: Commit**

```bash
git add src/input/joypad_map.h src/input/joypad_map.cpp test/test_joypad.cpp test/Makefile
git commit -m "M7: add host-tested joypad id -> gamepad button mapping

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: `Gamepad` class

**Files:**
- Create: `src/input/gamepad.h`
- Create: `src/input/gamepad.cpp`
- Modify: `Makefile` (`OBJS`, `EXTRACLEAN`)

- [ ] **Step 1: Write the Gamepad header**

Create `src/input/gamepad.h`:

```cpp
//
// src/input/gamepad.h
//
// Bare Metal Sega Genesis
// Wraps Circle's generic USB gamepad: find it, poll its normalized button
// bitmask. Knows nothing about libretro.
//

#ifndef _input_gamepad_h
#define _input_gamepad_h

#include <circle/devicenameservice.h>
#include <circle/usb/usbgamepad.h>
#include <circle/types.h>

class Gamepad
{
public:
    Gamepad(CDeviceNameService *pNameService);

    boolean  Initialize(void);     // find "upad1"; FALSE if no pad present
    boolean  IsPresent(void) const;
    void     Poll(void);           // snapshot GetReport()->buttons
    unsigned Buttons(void) const;  // cached TGamePadButton bitmask (0 if none)

private:
    CDeviceNameService *m_pNameService;
    CUSBGamePadDevice  *m_pDevice;
    unsigned            m_Buttons;
};

#endif
```

- [ ] **Step 2: Write the Gamepad implementation**

Create `src/input/gamepad.cpp`:

```cpp
//
// src/input/gamepad.cpp
//
// Bare Metal Sega Genesis
// See gamepad.h.
//

#include "gamepad.h"
#include "joypad_map.h"

// The pure mapping mirrors Circle's TGamePadButton bits; verify they match so
// the host-tested constants can never drift from the driver.
static_assert(GP_A == GamePadButtonA,         "GP_A bit mismatch");
static_assert(GP_B == GamePadButtonB,         "GP_B bit mismatch");
static_assert(GP_X == GamePadButtonX,         "GP_X bit mismatch");
static_assert(GP_Y == GamePadButtonY,         "GP_Y bit mismatch");
static_assert(GP_LB == GamePadButtonLB,       "GP_LB bit mismatch");
static_assert(GP_RB == GamePadButtonRB,       "GP_RB bit mismatch");
static_assert(GP_START == GamePadButtonStart, "GP_START bit mismatch");
static_assert(GP_SELECT == GamePadButtonSelect, "GP_SELECT bit mismatch");
static_assert(GP_UP == GamePadButtonUp,       "GP_UP bit mismatch");
static_assert(GP_DOWN == GamePadButtonDown,   "GP_DOWN bit mismatch");
static_assert(GP_LEFT == GamePadButtonLeft,   "GP_LEFT bit mismatch");
static_assert(GP_RIGHT == GamePadButtonRight, "GP_RIGHT bit mismatch");

Gamepad::Gamepad(CDeviceNameService *pNameService)
:   m_pNameService(pNameService), m_pDevice(0), m_Buttons(0)
{
}

boolean Gamepad::Initialize(void)
{
    m_pDevice = (CUSBGamePadDevice *)
        m_pNameService->GetDevice("upad", 1, FALSE);
    return m_pDevice != 0;
}

boolean Gamepad::IsPresent(void) const
{
    return m_pDevice != 0;
}

void Gamepad::Poll(void)
{
    if (m_pDevice != 0)
    {
        const TGamePadState *pState = m_pDevice->GetReport();
        m_Buttons = pState != 0 ? (unsigned) pState->buttons : 0;
    }
    else
    {
        m_Buttons = 0;
    }
}

unsigned Gamepad::Buttons(void) const
{
    return m_Buttons;
}
```

- [ ] **Step 3: Add the objects to the Makefile**

In `Makefile`, append to `OBJS` (currently ends at `src/audio/audio_driver.o`):

```makefile
       src/audio/audio_driver.o \
       src/input/joypad_map.o \
       src/input/gamepad.o
```

And extend `EXTRACLEAN`:

```makefile
EXTRACLEAN = src/*.o src/*.d src/storage/*.o src/storage/*.d \
             src/libretro/*.o src/libretro/*.d \
             src/video/*.o src/video/*.d \
             src/audio/*.o src/audio/*.d \
             src/input/*.o src/input/*.d \
             build/genesis libs/libgenesis.a
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: compiles `src/input/joypad_map.o` and `src/input/gamepad.o` (the `static_assert`s pass), links, ends with `COPY  kernel7.img`. No errors.

- [ ] **Step 5: Commit**

```bash
git add src/input/gamepad.h src/input/gamepad.cpp Makefile
git commit -m "M7: add Gamepad wrapping Circle CUSBGamePadDevice

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Wire the input callbacks

**Files:**
- Modify: `src/libretro/callbacks.h`
- Modify: `src/libretro/callbacks.cpp`

- [ ] **Step 1: Declare `g_gamepad` in the callbacks header**

In `src/libretro/callbacks.h`, after the `g_audio` block, add:

```cpp
// Set by the kernel before the frame loop; the input callbacks read the pad
// here. Forward-declared to avoid pulling Circle into this header.
class Gamepad;
extern Gamepad *g_gamepad;
```

- [ ] **Step 2: Define `g_gamepad` and implement the input callbacks**

In `src/libretro/callbacks.cpp`, add the includes after the existing ones:

```cpp
#include "../input/gamepad.h"
#include "../input/joypad_map.h"
```

Add the definition near the other globals:

```cpp
Gamepad *g_gamepad = 0;
```

Replace the no-op input stubs:

```cpp
void input_poll_cb(void)
{
}

int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                       unsigned id)
{
    (void)port; (void)device; (void)index; (void)id;
    return 0;
}
```

with:

```cpp
void input_poll_cb(void)
{
    if (g_gamepad != 0)
    {
        g_gamepad->Poll();
    }
}

int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                       unsigned id)
{
    (void)index;
    if (port != 0 || device != RETRO_DEVICE_JOYPAD || g_gamepad == 0)
    {
        return 0;
    }
    return joypad_state(g_gamepad->Buttons(), id);
}
```

- [ ] **Step 3: Build to verify it compiles and links**

Run: `make`
Expected: rebuilds `src/libretro/callbacks.o`, links cleanly, ends with `COPY  kernel7.img`.

- [ ] **Step 4: Commit**

```bash
git add src/libretro/callbacks.h src/libretro/callbacks.cpp
git commit -m "M7: forward libretro input callbacks to the Gamepad

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Kernel integration

**Files:**
- Modify: `src/kernel.h`
- Modify: `src/kernel.cpp`

- [ ] **Step 1: Add the include and member in kernel.h**

In `src/kernel.h`, add the include alongside the other project includes (after `#include "video/display.h"`):

```cpp
#include "input/gamepad.h"
```

Add the member to `CKernel`'s private section, immediately after `Display m_Display;`:

```cpp
	Display            m_Display;    // HDMI video output (M5)
	Gamepad            m_Gamepad;    // USB controller input (M7)
```

- [ ] **Step 2: Construct the member in kernel.cpp**

In `src/kernel.cpp`, add `m_Gamepad` to the constructor init list. It is declared after `m_Display` and before `m_SDCard`, so it must be initialised **before `m_SDCard`** (init-list order must match declaration order or `-Wreorder` fires). `m_Display` isn't in the init list (default-constructed), so insert `m_Gamepad` between the `m_Audio` and `m_SDCard` entries:

```cpp
	m_Audio (&m_Interrupt),
	m_Gamepad (&m_DeviceNameService),
	m_SDCard (m_FileSystem, m_DeviceNameService),
	m_pROMBuffer (0),
	m_nROMSize (0)
```

(`m_DeviceNameService` is declared early, so it is fully constructed before `m_Gamepad`.)

- [ ] **Step 3: Initialize the gamepad in Initialize()**

In `src/kernel.cpp`, in `CKernel::Initialize()`, add a block after the video init block (the one logging "Initialising video"), before `return bOK;`:

```cpp
	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising input");
		if (!m_Gamepad.Initialize ())
		{
			m_Logger.Write (FromKernel, LogNotice, "No USB gamepad found");
		}
		// Not fatal: the game still runs without a controller.
	}
```

(Note: do not set `bOK` from the gamepad — a missing pad must not fail boot.)

- [ ] **Step 4: Set 6-button mode and `g_gamepad` in Run()**

In `src/kernel.cpp` `CKernel::Run()`, find where the video callback is pointed at the display (`g_display = &m_Display;`) and add the controller setup just before it:

```cpp
	// Configure port 0 as a 6-button Genesis pad (port 1 unused), and point
	// the input callbacks at our gamepad.
	#define RETRO_DEVICE_MDPAD_6B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
	retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
	retro_set_controller_port_device (1, RETRO_DEVICE_NONE);
	g_gamepad = &m_Gamepad;

	// Point the video callback at our Display.
	g_display = &m_Display;
```

- [ ] **Step 5: Build to verify it compiles and links**

Run: `make`
Expected: rebuilds `src/kernel.o`, links cleanly, ends with `COPY  kernel7.img`. No errors.

- [ ] **Step 6: Confirm host tests still pass (no regression)**

Run: `make -C test run`
Expected: `All blit tests passed` and `All joypad tests passed`.

- [ ] **Step 7: Commit**

```bash
git add src/kernel.h src/kernel.cpp
git commit -m "M7: init gamepad, set 6-button port, wire g_gamepad

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Hardware acceptance

**Files:**
- Create: `docs/m7-hardware-setup.md`

This task has no automated test — it is the on-hardware verification.

- [ ] **Step 1: Document the acceptance checklist**

Create `docs/m7-hardware-setup.md`:

```markdown
# M7 Controller Input — Acceptance

## Setup
- Flash the M7 `kernel7.img`.
- Plug a USB gamepad into the Pi **before powering on** (hotplug is not
  supported in M7).
- A test Genesis ROM as `GAME.MD` on the card.

## Acceptance checklist
- [ ] All four D-pad directions move the character/menu correctly.
- [ ] The six face buttons (Genesis A/B/C/X/Y/Z) each do something in a game
      that uses them.
- [ ] Start pauses / starts; Mode (mapped to Select) behaves as Mode.
- [ ] With NO gamepad connected, the game still boots and runs; the serial log
      shows "No USB gamepad found".

## If a button is wrong / missing
- Wrong A/B/C feel: that is the core's standard 6-button mapping; remapping is a
  later milestone.
- D-pad dead on a particular pad: that pad may report the hat/axes without the
  normalized direction bits — a known deferred limitation (raw hat/axis
  fallback).
- No buttons at all: confirm the pad enumerated (it registers as "upad1"); try a
  different USB pad; ensure it was connected at boot.
```

- [ ] **Step 2: Commit**

```bash
git add docs/m7-hardware-setup.md
git commit -m "M7: document controller input on-hardware acceptance

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

- [ ] **Step 3: Flash and verify on hardware (manual)**

Copy `kernel7.img` to the card, connect a USB gamepad, boot, and work through
the acceptance checklist.

---

## Self-Review Notes

- **Spec coverage:** `Gamepad` wrapping `CUSBGamePadDevice` (Task 2) ✓; pure host-tested `joypad_state` mapping (Task 1) ✓; `g_gamepad` + input callbacks, port/device guards (Task 3) ✓; `m_Gamepad` init (non-fatal no-pad), 6-button via `retro_set_controller_port_device`, `g_gamepad` set (Task 4) ✓; on-hardware acceptance incl. no-pad case (Task 5) ✓; host test + blit regression (Tasks 1, 4) ✓.
- **Type consistency:** `joypad_state(unsigned, unsigned)->int16_t`, `Gamepad::Initialize/IsPresent/Poll/Buttons`, `g_gamepad`, and the `GP_*`/`GamePadButton*` static_asserts match across tasks. `GetDevice("upad", 1, FALSE)` matches the prefix/index overload.
- **No placeholders:** `RETRO_DEVICE_MDPAD_6B` defined concretely; all code blocks complete.
```
