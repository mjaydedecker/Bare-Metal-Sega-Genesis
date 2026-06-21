# In-Game Hotkeys + Toasts Implementation Plan (Spec B1)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add fixed Select+button in-game hotkeys (quick-save/load, volume ±, toggle HUD, mute) with transient on-screen toasts, reusing the Spec A overlay.

**Architecture:** A pure host-tested `decode_hotkey()` (`src/input/hotkey.*`) maps a (held, pressed) bitmask pair to an `InGameAction`. The kernel play loop reads player-1 button edges, decodes, executes the action via existing live paths, and shows a toast. `Overlay` gains a toast channel (independent of the HUD flag). A 2-line guard in `input_state_cb` suppresses player-1 input while Select is held.

**Tech Stack:** C++ bare-metal Circle (device) + host C++ unit tests via `test/Makefile`. RGB565 framebuffer; `TextCanvas` for drawing.

## Global Constraints

- `src/input/hotkey.*` decoder is **pure** (host-testable). `hotkey.h` declares only the enum + function (no includes beyond none needed); `hotkey.cpp` includes `joypad_map.h` for the `GP_*` bits.
- In-game hotkeys read **player 1 only** (`Gamepad::Buttons(0)`), consistent with the port-0 input suppression. Track a dedicated player-1 edge (`prevP1`), separate from the menu-hotkey `prevBtns`/`MenuButtons()`.
- Fixed mapping (no new settings): `Select+X` save, `Select+Y` load, `Select+A` toggle HUD, `Select+B` mute, `Select+Up` vol+, `Select+Down` vol-.
- Volume step ±10 clamped 0–100 (same as the settings screen). Volume/mute/HUD changes persist via `m_SettingsStore.Save(m_Settings)`. Quick-save/load use slot 1.
- No `snprintf` in device code — manual integer formatting (mirror `fmt_volume`).
- New kernel objects go in the root `Makefile` `OBJS` list. No changes to `Display::Blit`/`Present`.
- Host tests run with `make -C test run`; device build is `make`.

---

### Task 1: `decode_hotkey()` pure module + host test

**Files:**
- Create: `src/input/hotkey.h`, `src/input/hotkey.cpp`
- Create: `test/test_hotkey.cpp`
- Modify: `test/Makefile` (add `test_hotkey` target + to `run`/`clean`)
- Modify: `Makefile` (`OBJS`: add `src/input/hotkey.o`)

**Interfaces:**
- Consumes: `GP_*` bits from `joypad_map.h`; `hotkey_mask`/`MenuHotkey` (test only).
- Produces: `enum class InGameAction { None, QuickSave, QuickLoad, VolUp, VolDown, ToggleHud, Mute };` and `InGameAction decode_hotkey(unsigned held, unsigned pressed);`.

- [ ] **Step 1: Write `src/input/hotkey.h`**

```cpp
//
// src/input/hotkey.h
//
// Bare Metal Sega Genesis
// Pure decoder: maps a (held, pressed) GP_* bitmask pair to an in-game action
// for the fixed Select+button hotkey scheme. No Circle dependency.
//

#ifndef _input_hotkey_h
#define _input_hotkey_h

enum class InGameAction
{
    None, QuickSave, QuickLoad, VolUp, VolDown, ToggleHud, Mute
};

// held    = current GP_* bitmask this frame.
// pressed = newly-pressed edge bits this frame.
// Returns the mapped action when Select is held and the action button is freshly
// pressed; otherwise None.
InGameAction decode_hotkey(unsigned held, unsigned pressed);

#endif
```

- [ ] **Step 2: Write the failing test `test/test_hotkey.cpp`**

```cpp
#include "../src/input/hotkey.h"
#include "../src/input/joypad_map.h"   // GP_*, hotkey_mask, MenuHotkey
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // Each combo fires when Select held + action freshly pressed.
    assert(decode_hotkey(GP_SELECT | GP_X,    GP_X)    == InGameAction::QuickSave);
    assert(decode_hotkey(GP_SELECT | GP_Y,    GP_Y)    == InGameAction::QuickLoad);
    assert(decode_hotkey(GP_SELECT | GP_A,    GP_A)    == InGameAction::ToggleHud);
    assert(decode_hotkey(GP_SELECT | GP_B,    GP_B)    == InGameAction::Mute);
    assert(decode_hotkey(GP_SELECT | GP_UP,   GP_UP)   == InGameAction::VolUp);
    assert(decode_hotkey(GP_SELECT | GP_DOWN, GP_DOWN) == InGameAction::VolDown);

    // Edge semantics: action held but not freshly pressed -> None.
    assert(decode_hotkey(GP_SELECT | GP_X, 0) == InGameAction::None);
    // Action pressed without Select held -> None.
    assert(decode_hotkey(GP_X, GP_X) == InGameAction::None);

    // No collision with any menu_hotkey preset (held == pressed == preset mask).
    MenuHotkey presets[4] = { MenuHotkey::StartSelect, MenuHotkey::StartA,
                              MenuHotkey::StartB, MenuHotkey::LR };
    for (int i = 0; i < 4; i++)
    {
        unsigned m = hotkey_mask(presets[i]);
        assert(decode_hotkey(m, m) == InGameAction::None);
    }

    printf("All hotkey tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the `test_hotkey` target to `test/Makefile`**

Add `test_hotkey` to the `run:` prerequisites and a `./test_hotkey` line in its recipe; add `test_hotkey` to the `clean:` `rm -f` list; and add the rule (it links `joypad_map.cpp` for `hotkey_mask`, needing the libretro include like `test_joypad`):

```make
test_hotkey: test_hotkey.cpp ../src/input/hotkey.cpp ../src/input/hotkey.h ../src/input/joypad_map.cpp
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_hotkey.cpp ../src/input/hotkey.cpp ../src/input/joypad_map.cpp
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test test_hotkey`
Expected: FAIL — `../src/input/hotkey.cpp` missing / `decode_hotkey` undefined.

- [ ] **Step 5: Write `src/input/hotkey.cpp`**

```cpp
//
// src/input/hotkey.cpp
//
// Bare Metal Sega Genesis
// See hotkey.h.
//

#include "hotkey.h"
#include "joypad_map.h"   // GP_* bits

InGameAction decode_hotkey(unsigned held, unsigned pressed)
{
    if (!(held & GP_SELECT)) return InGameAction::None;   // modifier required

    if (pressed & GP_X)    return InGameAction::QuickSave;
    if (pressed & GP_Y)    return InGameAction::QuickLoad;
    if (pressed & GP_A)    return InGameAction::ToggleHud;
    if (pressed & GP_B)    return InGameAction::Mute;
    if (pressed & GP_UP)   return InGameAction::VolUp;
    if (pressed & GP_DOWN) return InGameAction::VolDown;
    return InGameAction::None;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test test_hotkey && ./test/test_hotkey`
Expected: PASS — `All hotkey tests passed`.

- [ ] **Step 7: Add `src/input/hotkey.o` to the kernel build**

In the root `Makefile` `OBJS` list, after `src/input/pad_reconcile.o` add a continuation:

```make
       src/input/pad_reconcile.o \
       src/input/hotkey.o
```

(Ensure the previous line ends with ` \`.)

- [ ] **Step 8: Commit**

```bash
git add src/input/hotkey.h src/input/hotkey.cpp test/test_hotkey.cpp test/Makefile Makefile
git commit -m "Input: add pure decode_hotkey() for in-game Select+button combos"
```

---

### Task 2: Toast channel on `Overlay`

**Files:**
- Modify: `src/ui/overlay.h` (constants, members, methods)
- Modify: `src/ui/overlay.cpp` (constructor init, `ShowToast`, `DrawToast`)

**Interfaces:**
- Consumes: `TextCanvas` (`CharW/CharH/Cols/Rows/FillRect/DrawText`).
- Produces: `void Overlay::ShowToast(const char *msg)` and `void Overlay::DrawToast(void)` (independent of the HUD `enabled` flag).

- [ ] **Step 1: Add constants, members, and method declarations in `src/ui/overlay.h`**

Add the constants near the top (after the includes):

```cpp
#define TOAST_MAX    28
#define TOAST_FRAMES 120   // ~2 s at 60 fps
```

Add the public methods (after `void Draw(const HudStats &s);`):

```cpp
    void Draw(const HudStats &s);   // no-op if disabled

    void ShowToast(const char *msg);  // start a transient bottom-center message
    void DrawToast(void);             // every frame: draw if active, then decay
```

Add the members (after `bool m_Enabled;`):

```cpp
    TextCanvas *m_pCanvas;
    bool        m_Enabled;
    char        m_Toast[TOAST_MAX + 1];
    unsigned    m_ToastFrames;
```

- [ ] **Step 2: Initialize the toast state in the constructor**

In `src/ui/overlay.cpp`, update the constructor:

```cpp
Overlay::Overlay(TextCanvas *pCanvas)
:   m_pCanvas(pCanvas), m_Enabled(false), m_ToastFrames(0)
{
    m_Toast[0] = '\0';
}
```

- [ ] **Step 3: Implement `ShowToast` and `DrawToast`**

Append to `src/ui/overlay.cpp`:

```cpp
void Overlay::ShowToast(const char *msg)
{
    unsigned i = 0;
    if (msg != 0)
        for (; msg[i] != '\0' && i < TOAST_MAX; i++) m_Toast[i] = msg[i];
    m_Toast[i] = '\0';
    m_ToastFrames = TOAST_FRAMES;
}

void Overlay::DrawToast(void)
{
    if (m_ToastFrames == 0) return;
    m_ToastFrames--;

    int cw   = (int) m_pCanvas->CharW();
    int ch   = (int) m_pCanvas->CharH();
    int cols = (int) m_pCanvas->Cols();
    int rows = (int) m_pCanvas->Rows();

    int len = 0; while (m_Toast[len] != '\0') len++;

    // Opaque box, centered horizontally, near the bottom (HUD is top-left).
    int boxW = (len + 2) * cw;
    int boxH = 2 * ch;
    int x = (cols * cw - boxW) / 2;
    if (x < 0) x = 0;
    int y = (rows - 3) * ch;
    m_pCanvas->FillRect(x, y, boxW, boxH, 0x0000);
    m_pCanvas->DrawText(x + cw, y + ch / 2, m_Toast, 0xFFFF, 0x0000);
}
```

- [ ] **Step 4: Build to verify it compiles**

Run: `make`
Expected: builds to completion; `src/ui/overlay.o` recompiles cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/ui/overlay.h src/ui/overlay.cpp
git commit -m "UI: add transient toast channel to Overlay (independent of HUD)"
```

---

### Task 3: Input suppression in `input_state_cb`

**Files:**
- Modify: `src/libretro/callbacks.cpp` (`input_state_cb`)

**Interfaces:**
- Consumes: `GP_SELECT` (already available via `joypad_map.h`, included by `callbacks.cpp`).
- Produces: player-1 input masked while its Select is held. No host test (device-only).

- [ ] **Step 1: Add the suppression guard**

In `src/libretro/callbacks.cpp`, replace the tail of `input_state_cb` (currently `return joypad_state(g_gamepad->Buttons(port), id, *map);`):

```cpp
    unsigned buttons = g_gamepad->Buttons(port);
    if (port == 0 && (buttons & GP_SELECT))   // player-1 hotkey mode: mask input
    {
        return 0;
    }
    return joypad_state(buttons, id, *map);
```

(Confirm `#include "../input/joypad_map.h"` is already present at the top of the file — it is, for `joypad_state`/`GP_*`.)

- [ ] **Step 2: Build to verify it compiles**

Run: `make`
Expected: builds to completion; `src/libretro/callbacks.o` recompiles cleanly.

- [ ] **Step 3: Commit**

```bash
git add src/libretro/callbacks.cpp
git commit -m "Input: suppress player-1 input to core while Select held (hotkey mode)"
```

---

### Task 4: Kernel dispatch — decode, execute, toast, draw

**Files:**
- Modify: `src/kernel.cpp` (include, `vol_toast` helper, `prevP1`, dispatch block, `DrawToast` call)

**Interfaces:**
- Consumes: `decode_hotkey`/`InGameAction` (Task 1), `Overlay::ShowToast`/`DrawToast` (Task 2), `m_SaveState.Save/Load/Occupied`, `m_Audio.SetVolume/SetMute`, `m_SettingsStore.Save`, `m_Settings`.
- Produces: live in-game hotkey actions + toasts. No host test (device-only loop wiring).

- [ ] **Step 1: Include the decoder**

In `src/kernel.cpp`, after the existing `#include "input/joypad_map.h"` line, add:

```cpp
#include "input/joypad_map.h"   // GP_START, GP_SELECT bits for the menu hotkey
#include "input/hotkey.h"       // decode_hotkey, InGameAction
```

- [ ] **Step 2: Add a `vol_toast` helper**

Next to the existing `scale_name` static (near the top of `kernel.cpp`), add:

```cpp
// Format "Volume NNN" into out (>= 12 bytes) without snprintf.
static void vol_toast (char *out, unsigned v)
{
	const char *p = "Volume ";
	int i = 0;
	while (*p) out[i++] = *p++;
	char rev[4];
	int  n = 0;
	if (v == 0) rev[n++] = '0';
	else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
	while (n) out[i++] = rev[--n];
	out[i] = '\0';
}
```

- [ ] **Step 3: Add a player-1 edge tracker before the play loop**

In the `--- Play ---` setup, next to `unsigned prevBtns = 0;` (line ~258), add:

```cpp
		unsigned prevBtns  = 0;
		unsigned prevP1    = 0;   // player-1 button state for in-game hotkeys
```

- [ ] **Step 4: Add the dispatch block after the menu-hotkey block**

In `src/kernel.cpp`, immediately after the menu-hotkey `if (...) { ... continue; }` block closes (the `}` at line ~287, before `retro_run ();`), insert:

```cpp
			// In-game action hotkeys (player 1: Select + button), read live.
			unsigned p1now     = m_Gamepad.Buttons (0);
			unsigned p1pressed = p1now & ~prevP1;
			prevP1 = p1now;
			switch (decode_hotkey (p1now, p1pressed))
			{
			case InGameAction::QuickSave:
				m_Overlay.ShowToast (m_SaveState.Save (1) ? "Quick-saved"
				                                          : "Save failed");
				break;
			case InGameAction::QuickLoad:
				if (!m_SaveState.Occupied (1))
					m_Overlay.ShowToast ("No quick save");
				else
					m_Overlay.ShowToast (m_SaveState.Load (1) ? "Quick-loaded"
					                                          : "Load failed");
				break;
			case InGameAction::VolUp:
			case InGameAction::VolDown:
			{
				int dir = (decode_hotkey (p1now, p1pressed) == InGameAction::VolUp)
				          ? 10 : -10;
				int v = (int) m_Settings.volume + dir;
				if (v < 0)   v = 0;
				if (v > 100) v = 100;
				m_Settings.volume = (unsigned) v;
				m_Audio.SetVolume (m_Settings.volume);
				m_SettingsStore.Save (m_Settings);
				char t[12];
				vol_toast (t, m_Settings.volume);
				m_Overlay.ShowToast (t);
				break;
			}
			case InGameAction::ToggleHud:
				m_Settings.debug_overlay = !m_Settings.debug_overlay;
				m_Overlay.SetEnabled (m_Settings.debug_overlay);
				m_SettingsStore.Save (m_Settings);
				m_Overlay.ShowToast (m_Settings.debug_overlay ? "HUD on" : "HUD off");
				if (!m_Settings.debug_overlay) m_Display.ForceRepaint ();
				break;
			case InGameAction::Mute:
				m_Settings.mute = !m_Settings.mute;
				m_Audio.SetMute (m_Settings.mute);
				m_SettingsStore.Save (m_Settings);
				m_Overlay.ShowToast (m_Settings.mute ? "Muted" : "Unmuted");
				break;
			case InGameAction::None:
				break;
			}
```

(The `VolUp`/`VolDown` cases share a body; `decode_hotkey` is cheap and pure, so re-calling it to pick the direction keeps the block self-contained. Alternatively store the action in a local — either is fine.)

- [ ] **Step 5: Draw the toast every frame**

After the HUD draw block (the `if (m_Overlay.Enabled ()) { ... m_Overlay.Draw (st); }`), before `next += period_us;`, add:

```cpp
			m_Overlay.DrawToast ();   // transient toast, independent of HUD
```

- [ ] **Step 6: Build + run the full host suite**

Run: `make && make -C test run`
Expected: kernel builds; all host suites pass (`All hotkey tests passed`, etc.), no regressions.

- [ ] **Step 7: Commit**

```bash
git add src/kernel.cpp
git commit -m "Kernel: dispatch in-game hotkeys (save/load/vol/HUD/mute) with toasts"
```

---

### Task 5: Hardware-verify checklist (section N)

**Files:**
- Modify: `docs/hardware-verification-checklist-2026-06-20.md`

**Interfaces:** none (documentation).

- [ ] **Step 1: Append section N and a summary row**

After the `## M. Diagnostics HUD (debug_overlay)` section (before `## Results summary`), add:

```markdown
## N. In-game hotkeys + toasts

- [ ] **N1 — Quick-save/load.** In-game: `Select+X` shows `Quick-saved`; `Select+Y`
  shows `Quick-loaded` and restores the state. On a fresh game `Select+Y` (empty
  slot 1) shows `No quick save`.
- [ ] **N2 — Volume.** `Select+Up`/`Select+Down` change volume live; toast shows
  `Volume NN`; the level persists across reboot.
- [ ] **N3 — HUD + mute.** `Select+A` toggles the diagnostics HUD (toast `HUD on`/
  `HUD off`, no ghost when turned off); `Select+B` toggles mute (`Muted`/
  `Unmuted`). Both persist across reboot.
- [ ] **N4 — Suppression.** While Select is held, the game receives no player-1
  input; releasing Select restores normal play. Player 2 is unaffected.
- [ ] **N5 — Toast lifetime.** Each toast appears bottom-center and disappears
  after ~2 s with no leftover pixels over the game.
- [ ] **N6 — No menu collision.** The configured `menu_hotkey` still opens the
  pause menu and does not trigger an in-game action.
```

In the Results summary table, add after the `| M. Diagnostics HUD | | |` row:

```markdown
| N. Hotkeys + toasts | | |
```

- [ ] **Step 2: Commit**

```bash
git add docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Docs: add in-game hotkeys + toasts hardware-verify checklist (section N)"
```

---

## Self-Review Notes

- **Spec coverage:** decoder + mapping → Task 1; toast channel → Task 2; input suppression → Task 3; action dispatch/execution/persistence + toast draw → Task 4; checklist N → Task 5. All spec sections mapped.
- **Type consistency:** `InGameAction` enumerators and `decode_hotkey(unsigned, unsigned)` identical across Tasks 1 and 4. `ShowToast(const char*)`/`DrawToast(void)` identical across Tasks 2 and 4.
- **Player-1 consistency:** the decoder reads `Buttons(0)` with its own `prevP1` edge (Task 4), matching the port-0 suppression (Task 3) — both target player 1 only.
- **No Blit/Display changes:** toast and HUD draw via `TextCanvas` on the front page; HUD-off mid-game calls the existing `ForceRepaint` to clear the top-left box. Vsync/pacing untouched.
