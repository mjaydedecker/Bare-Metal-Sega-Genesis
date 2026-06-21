# Controller Hotplug-Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `Gamepad::Poll()` react to a USB controller being unplugged — zero the removed port's cached buttons (no ghost inputs) and let a re-plug re-register on the same port.

**Architecture:** Each frame, `Poll()` queries `GetDevice("upad", i+1)` for every port and compares it to the cached device pointer. The pure decision (Keep / Clear / Acquire) is factored into a host-tested helper `pad_reconcile()`; `Poll()` performs the Circle-side effects (zero cache, drop slot, register handler). Removal is detectable because `CUSBGamePadDevice`'s destructor removes the `upad` name entry, so `GetDevice` returns NULL after unplug.

**Tech Stack:** C++ (no RTTI/exceptions), Circle bare-metal (Raspberry Pi 2, AArch32). Host unit tests built with the system `c++`.

## Global Constraints

- Cross build target: Raspberry Pi 2, AArch32, `PREFIX = arm-linux-gnueabihf-`, output `kernel7.img` (do not change). Build with `make -j4` from repo root.
- Host tests: built/run from `test/` with `make -C test run` (system `c++`, not the cross toolchain).
- Behavior: **silent neutral + auto re-acquire**; a re-plugged pad returns to the same port (Circle reuses the lowest free `upad` index).
- No change to `Gamepad`'s public API (`Buttons`, `MenuButtons`, `IsPresent`).
- Pure helpers carry no Circle dependency so they compile on the host (mirror `joypad_map`, `audio_util`, `settings`).

---

### Task 1: Pure `pad_reconcile` decision helper (host-tested)

**Files:**
- Create: `src/input/pad_reconcile.h`
- Create: `src/input/pad_reconcile.cpp`
- Test: `test/test_pad_reconcile.cpp`
- Modify: `test/Makefile` (add the new test to `run`, a build rule, and `clean`)

**Interfaces:**
- Produces: `enum class PadAction { Keep, Clear, Acquire };` and
  `PadAction pad_reconcile(const void *cached, const void *queried);`
  - `queried == 0 && cached != 0` → `Clear` (removed)
  - `queried == 0 && cached == 0` → `Keep` (still absent)
  - `queried != 0 && queried == cached` → `Keep` (unchanged)
  - `queried != 0 && queried != cached` → `Acquire` (first plug / re-plug / swap)

- [ ] **Step 1: Write the failing test**

Create `test/test_pad_reconcile.cpp`:

```cpp
#include "../src/input/pad_reconcile.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    int a, b;   // distinct addresses serve as fake device pointers

    // Never present, still absent -> nothing to do.
    assert(pad_reconcile(0, 0) == PadAction::Keep);

    // First plug-in: nothing cached, a device appears.
    assert(pad_reconcile(0, &a) == PadAction::Acquire);

    // Unchanged: the same device is still present.
    assert(pad_reconcile(&a, &a) == PadAction::Keep);

    // Removed: we had a device, now GetDevice returns NULL.
    assert(pad_reconcile(&a, 0) == PadAction::Clear);

    // Re-plug / swap at the same index: a different pointer.
    assert(pad_reconcile(&a, &b) == PadAction::Acquire);

    printf("test_pad_reconcile: OK\n");
    return 0;
}
```

- [ ] **Step 2: Add the test to `test/Makefile`**

In the `run:` target's dependency list and command list, append `test_pad_reconcile`. Add a build rule and extend `clean`:

```make
test_pad_reconcile: test_pad_reconcile.cpp ../src/input/pad_reconcile.cpp ../src/input/pad_reconcile.h
	$(CXX) $(CXXFLAGS) -o $@ test_pad_reconcile.cpp ../src/input/pad_reconcile.cpp
```

The `run:` line becomes:
```make
run: test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings test_audio_util test_pad_reconcile
```
…with `./test_pad_reconcile` added to the recipe, and `test_pad_reconcile` added to the `clean:` `rm -f` list.

- [ ] **Step 3: Run the test to verify it fails (no header yet)**

Run: `make -C test test_pad_reconcile`
Expected: FAIL — `fatal error: ../src/input/pad_reconcile.h: No such file or directory`.

- [ ] **Step 4: Write the header**

Create `src/input/pad_reconcile.h`:

```cpp
//
// src/input/pad_reconcile.h
//
// Bare Metal Sega Genesis
// Pure per-port hotplug decision: compare a controller port's cached device
// pointer against the pointer the name service currently returns. No Circle
// dependency so it can be host-tested; the caller performs the side effects.
//

#ifndef _input_pad_reconcile_h
#define _input_pad_reconcile_h

enum class PadAction
{
    Keep,      // no change (same device, or still absent)
    Clear,     // device removed -> zero the cache and drop the slot
    Acquire    // new device present (first plug, re-plug, or swap) -> register + cache
};

// cached  = the port's last-known device pointer (0 if none)
// queried = what GetDevice("upad", port+1) returns now (0 if absent/removed)
PadAction pad_reconcile(const void *cached, const void *queried);

#endif
```

- [ ] **Step 5: Write the implementation**

Create `src/input/pad_reconcile.cpp`:

```cpp
//
// src/input/pad_reconcile.cpp
//
// Bare Metal Sega Genesis
// See pad_reconcile.h.
//

#include "pad_reconcile.h"

PadAction pad_reconcile(const void *cached, const void *queried)
{
    if (queried == 0)
    {
        return cached != 0 ? PadAction::Clear : PadAction::Keep;
    }
    return queried != cached ? PadAction::Acquire : PadAction::Keep;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test run`
Expected: all tests run; final line `test_pad_reconcile: OK` and no assertion failures.

- [ ] **Step 7: Commit**

```bash
git add src/input/pad_reconcile.h src/input/pad_reconcile.cpp test/test_pad_reconcile.cpp test/Makefile
git commit -m "Input: pure pad_reconcile hotplug-decision helper + host test"
```

---

### Task 2: Wire `pad_reconcile` into `Gamepad::Poll()` and the kernel build

**Files:**
- Modify: `src/input/gamepad.cpp` (add include; rewrite the `Poll()` loop)
- Modify: `Makefile` (add `src/input/pad_reconcile.o` to `OBJS`)

**Interfaces:**
- Consumes: `pad_reconcile(const void *cached, const void *queried)` and `PadAction` from Task 1.
- Produces: no public API change. `Poll()` now also clears a removed port and re-acquires a re-plugged one; `IsPresent(port)` consequently reflects live presence.

- [ ] **Step 1: Add `pad_reconcile.o` to the kernel `OBJS`**

In `Makefile`, in the `OBJS = …` list, add the object next to the other input objects:

```make
       src/input/joypad_map.o \
       src/input/gamepad.o \
       src/input/pad_reconcile.o \
```

(`DEPS = $(OBJS:.o=.d)` already picks up the new object's header deps.)

- [ ] **Step 2: Include the helper in `gamepad.cpp`**

At the top of `src/input/gamepad.cpp`, add the include next to the existing one:

```cpp
#include "gamepad.h"
#include "joypad_map.h"
#include "pad_reconcile.h"
```

- [ ] **Step 3: Rewrite the `Poll()` loop**

Replace the body of `Gamepad::Poll()` (the acquire-only `for` loop) with the reconcile loop:

```cpp
void Gamepad::Poll(void)
{
    static TGamePadStatusHandler *const handlers[MAX_PADS] = { Handler0, Handler1 };

    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        CUSBGamePadDevice *dev = (CUSBGamePadDevice *)
            m_pNameService->GetDevice("upad", i + 1, FALSE);

        switch (pad_reconcile(m_pDevice[i], dev))
        {
        case PadAction::Clear:           // unplugged: stop ghost inputs, free slot
            s_buttons[i] = 0;
            m_pDevice[i] = 0;
            break;

        case PadAction::Acquire:         // first plug / re-plug / swap
            m_pDevice[i] = dev;
            s_buttons[i] = 0;
            // Circle only decodes reports while a handler is registered.
            dev->RegisterStatusHandler(handlers[i]);
            break;

        case PadAction::Keep:            // same device, or still absent
            break;
        }
    }
}
```

- [ ] **Step 4: Cross-build the kernel**

Run: `make -j4`
Expected: compiles `src/input/pad_reconcile.o` and `src/input/gamepad.o`, links, ends with `COPY  kernel7.img` and `WC    kernel7.img => <size>`. No warnings/errors.

- [ ] **Step 5: Re-run host tests (guard against regressions)**

Run: `make -C test run`
Expected: all tests pass, including `test_pad_reconcile: OK`.

- [ ] **Step 6: Commit**

```bash
git add Makefile src/input/gamepad.cpp
git commit -m "Input: handle controller unplug in Poll() (no ghost inputs, auto re-acquire)"
```

- [ ] **Step 7: Manual hardware verification (record results)**

Flash `kernel7.img` and confirm:
1. Hold a D-pad direction, unplug the pad mid-press → on-screen motion stops within ~a frame (no stuck input).
2. Re-plug the same pad → it works again and drives the **same** port.
3. With two pads: unplug P2 → P1 unaffected; unplug P1 → P2 unaffected.
4. After a remove/re-plug cycle, either pad still opens the pause menu (`MenuButtons()`).

---

## Self-Review

**Spec coverage:**
- "Zero removed port's cache (no ghost inputs)" → Task 2, Step 3 `PadAction::Clear`. ✓
- "Clear slot so re-plug re-registers" → Task 2, Step 3 `Clear` sets `m_pDevice[i]=0`; `Acquire` re-registers. ✓
- "Poll-based reconcile, NULL after removal" → Task 2 `GetDevice` + Task 1 truth table. ✓
- "Silent, same-port restore, no API change" → no UI/API touched; `Acquire` writes the same slot index. ✓
- "Memory safety: never deref freed pointer" → `dev` is only dereferenced in `Acquire`, where it is a live pointer from `GetDevice`; `Clear` only compares/zeros. ✓
- "Hardware-tested, not unit-tested for Poll(); pure logic host-tested" → Task 1 host test for the decision; Task 2 Step 7 manual checks. ✓

**Placeholder scan:** No TBD/TODO/"handle edge cases"; every code step shows complete code. ✓

**Type consistency:** `PadAction { Keep, Clear, Acquire }` and `pad_reconcile(const void *, const void *)` are used identically in Task 1 (definition/test) and Task 2 (the `switch`). `handlers[]`, `s_buttons[]`, `m_pDevice[]` match existing `gamepad.cpp` declarations. ✓
