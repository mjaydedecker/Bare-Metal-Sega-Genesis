# Save States Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Save/load the full emulator state to four SD-card slots per game, driven from the in-emulation menu.

**Architecture:** A pure `state_path` helper (host-tested) builds `SD:/saves/<rom>.state<N>`. `Storage` gains write/query methods. A `SaveState` manager wraps `retro_serialize`/`retro_unserialize` + `Storage`. `PauseMenu` enables Save/Load State and runs a slot-picker sub-flow that calls `SaveState`. The kernel sets the current game on `SaveState` per load; the play loop is unchanged.

**Tech Stack:** C++ bare metal, Circle, ChaN FatFS, Genesis-Plus-GX-Wide libretro. Host tests with system `c++` in `test/`.

**Spec:** `docs/superpowers/specs/2026-06-20-save-states-design.md`

---

## File structure

| File | Responsibility |
|---|---|
| `src/menu/save_path.{h,cpp}` (new) | Pure `state_path(romPath, slot, out, n)`. |
| `src/storage/storage.{h,cpp}` (modify) | Add `WriteFile`, `Exists`, `MakeDir`. |
| `src/menu/save_state.{h,cpp}` (new) | `SaveState` manager (serialize/storage). |
| `src/menu/pause_menu.{h,cpp}` (modify) | Enable Save/Load; slot picker; messages. |
| `src/kernel.{h,cpp}` (modify) | `SaveState` member; `SetGame` per load. |
| `Makefile` (modify) | Add `save_path.o`, `save_state.o`. |
| `test/test_save_path.cpp` (new), `test/Makefile` (modify) | Host test. |

---

## Task 1: `state_path` (pure, host-tested)

**Files:**
- Create: `src/menu/save_path.h`, `src/menu/save_path.cpp`, `test/test_save_path.cpp`
- Modify: `test/Makefile`

- [ ] **Step 1: Write `src/menu/save_path.h`**

```cpp
//
// src/menu/save_path.h
//
// Bare Metal Sega Genesis
// Pure: build the save-state file path for a ROM + slot.
//

#ifndef _menu_save_path_h
#define _menu_save_path_h

// out = "SD:/saves/<filename>.state<slot>", where <filename> is romPath with its
// directory stripped (extension kept) and slot is 1..4. Bounded by out_size.
void state_path(const char *romPath, int slot, char *out, unsigned out_size);

#endif
```

- [ ] **Step 2: Write the failing test `test/test_save_path.cpp`**

```cpp
#include "../src/menu/save_path.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    char out[300];

    state_path("SD:/roms/Sonic.md", 2, out, sizeof out);
    assert(strcmp(out, "SD:/saves/Sonic.md.state2") == 0);

    state_path("SD:/roms/Genesis/Streets of Rage.md", 1, out, sizeof out);
    assert(strcmp(out, "SD:/saves/Streets of Rage.md.state1") == 0);

    state_path("Game.bin", 4, out, sizeof out);   // no directory
    assert(strcmp(out, "SD:/saves/Game.bin.state4") == 0);

    printf("All save_path tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the test target to `test/Makefile`**

Update the `run` target to append `test_save_path`:

```make
run: test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path
	./test_blit
	./test_joypad
	./test_rom_filter
	./test_menu_state
	./test_menu_path
	./test_save_path
```

Add this target after `test_menu_path`:

```make
test_save_path: test_save_path.cpp ../src/menu/save_path.cpp ../src/menu/save_path.h
	$(CXX) $(CXXFLAGS) -o $@ test_save_path.cpp ../src/menu/save_path.cpp
```

Update the `clean` rule to include it:

```make
clean:
	rm -f test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test test_save_path`
Expected: FAIL — `undefined reference to state_path`.

- [ ] **Step 5: Write `src/menu/save_path.cpp`**

```cpp
//
// src/menu/save_path.cpp
//
// Bare Metal Sega Genesis
// See save_path.h.
//

#include "save_path.h"

static void append(char *out, unsigned out_size, unsigned *pos, const char *s)
{
    unsigned p = *pos;
    for (unsigned i = 0; s[i] != '\0' && p + 1 < out_size; i++) out[p++] = s[i];
    *pos = p;
}

void state_path(const char *romPath, int slot, char *out, unsigned out_size)
{
    if (out_size == 0) return;

    const char *base = romPath;            // basename: after the last '/'
    for (unsigned i = 0; romPath[i] != '\0'; i++)
    {
        if (romPath[i] == '/') base = &romPath[i + 1];
    }

    int s = slot;
    if (s < 0) s = 0;
    if (s > 9) s = 9;
    char digit[2] = { (char) ('0' + s), '\0' };

    unsigned pos = 0;
    append(out, out_size, &pos, "SD:/saves/");
    append(out, out_size, &pos, base);
    append(out, out_size, &pos, ".state");
    append(out, out_size, &pos, digit);
    out[pos] = '\0';
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test test_save_path && ./test/test_save_path`
Expected: `All save_path tests passed`

- [ ] **Step 7: Commit**

```bash
git add src/menu/save_path.h src/menu/save_path.cpp test/test_save_path.cpp test/Makefile
git commit -m "Save states: host-tested state_path"
```

---

## Task 2: Storage write/query methods

**Files:**
- Modify: `src/storage/storage.h`, `src/storage/storage.cpp`

- [ ] **Step 1: Declare the methods in `src/storage/storage.h`**

After the `ReadFile` declaration, add:

```cpp
    // Write an entire buffer to a file, creating/overwriting it.
    bool WriteFile(const char *path, const u8 *data, size_t size);

    // True if a file or directory exists at path.
    bool Exists(const char *path);

    // Create a directory (success if it already exists).
    bool MakeDir(const char *path);
```

- [ ] **Step 2: Implement them in `src/storage/storage.cpp`**

Append at the end of the file:

```cpp
bool Storage::WriteFile(const char *path, const u8 *data, size_t size)
{
    FIL file;
    if (f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        CLogger::Get()->Write(FromStorage, LogError, "Cannot create: %s", path);
        return false;
    }

    UINT written = 0;
    FRESULT r = f_write(&file, data, (UINT) size, &written);
    f_close(&file);

    if (r != FR_OK || written != size)
    {
        CLogger::Get()->Write(FromStorage, LogError, "Write error: %s", path);
        return false;
    }
    return true;
}

bool Storage::Exists(const char *path)
{
    FILINFO fno;
    return f_stat(path, &fno) == FR_OK;
}

bool Storage::MakeDir(const char *path)
{
    FRESULT r = f_mkdir(path);
    return r == FR_OK || r == FR_EXIST;
}
```

- [ ] **Step 3: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `storage.o`, links, exit 0.

- [ ] **Step 4: Commit**

```bash
git add src/storage/storage.h src/storage/storage.cpp
git commit -m "Save states: Storage WriteFile/Exists/MakeDir"
```

---

## Task 3: SaveState manager

Built and linked but not yet wired into the menu; cross-build verified.

**Files:**
- Create: `src/menu/save_state.h`, `src/menu/save_state.cpp`
- Modify: `Makefile`

- [ ] **Step 1: Write `src/menu/save_state.h`**

```cpp
//
// src/menu/save_state.h
//
// Bare Metal Sega Genesis
// Save/load the libretro core state to SD-card slots for the current game.
//

#ifndef _menu_save_state_h
#define _menu_save_state_h

#include <circle/types.h>
#include "../storage/storage.h"

class SaveState
{
public:
    SaveState(Storage *pStorage);

    void SetGame(const char *romPath);   // copies the current ROM path

    bool Occupied(int slot);             // a save file exists for this slot (1..4)
    bool Save(int slot);                 // serialize the core -> slot file
    bool Load(int slot);                 // slot file -> unserialize the core

private:
    Storage *m_pStorage;
    char     m_romPath[300];
};

#endif
```

- [ ] **Step 2: Write `src/menu/save_state.cpp`**

```cpp
//
// src/menu/save_state.cpp
//
// Bare Metal Sega Genesis
// See save_state.h.
//

#include "save_state.h"
#include "save_path.h"
#include <libretro.h>

static void copy_str(char *dst, const char *src, unsigned n)
{
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < n; i++) dst[i] = src[i];
    dst[i] = '\0';
}

SaveState::SaveState(Storage *pStorage)
:   m_pStorage(pStorage)
{
    m_romPath[0] = '\0';
}

void SaveState::SetGame(const char *romPath)
{
    copy_str(m_romPath, romPath, sizeof m_romPath);
}

bool SaveState::Occupied(int slot)
{
    char path[320];
    state_path(m_romPath, slot, path, sizeof path);
    return m_pStorage->Exists(path);
}

bool SaveState::Save(int slot)
{
    m_pStorage->MakeDir("SD:/saves");

    size_t size = retro_serialize_size();
    if (size == 0) return false;

    u8 *buf = new u8[size];
    if (!retro_serialize(buf, size))
    {
        delete[] buf;
        return false;
    }

    char path[320];
    state_path(m_romPath, slot, path, sizeof path);
    bool ok = m_pStorage->WriteFile(path, buf, size);
    delete[] buf;
    return ok;
}

bool SaveState::Load(int slot)
{
    char path[320];
    state_path(m_romPath, slot, path, sizeof path);

    u8    *buf  = 0;
    size_t size = 0;
    if (!m_pStorage->ReadFile(path, &buf, &size))
    {
        return false;
    }

    bool ok = retro_unserialize(buf, size);
    delete[] buf;
    return ok;
}
```

- [ ] **Step 3: Add the objects to `OBJS` in `Makefile`**

Replace:

```make
       src/menu/rom_menu.o \
       src/menu/pause_menu.o \
       src/ui/text_canvas.o
```

with:

```make
       src/menu/rom_menu.o \
       src/menu/pause_menu.o \
       src/menu/save_path.o \
       src/menu/save_state.o \
       src/ui/text_canvas.o
```

- [ ] **Step 4: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `save_path.o` and `save_state.o`, links, exit 0.

- [ ] **Step 5: Commit**

```bash
git add src/menu/save_state.h src/menu/save_state.cpp Makefile
git commit -m "Save states: SaveState manager (serialize/storage)"
```

---

## Task 4: PauseMenu save/load + kernel wiring

Enables the Save/Load entries, adds the slot picker, and wires `SaveState` into
the kernel. Changing `PauseMenu`'s constructor requires updating the kernel in the
same task so the build stays green.

**Files:**
- Modify: `src/menu/pause_menu.h`, `src/menu/pause_menu.cpp`
- Modify: `src/kernel.h`, `src/kernel.cpp`

- [ ] **Step 1: Update `src/menu/pause_menu.h`**

Replace the whole file with:

```cpp
//
// src/menu/pause_menu.h
//
// Bare Metal Sega Genesis
// In-emulation overlay menu. Pauses the caller; returns the chosen action.
//

#ifndef _menu_pause_menu_h
#define _menu_pause_menu_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "save_state.h"

enum class MenuAction { Resume, Reset, ReturnToBrowser };

class PauseMenu
{
public:
    PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
              SaveState *pSaveState);
    MenuAction Run(void);     // draws over the last frame; returns on confirm

private:
    void Render(int selected);
    int  PickSlot(bool forLoad);   // returns slot 1..4, or 0 to cancel
    void Message(const char *text);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    SaveState     *m_pSaveState;
};

#endif
```

- [ ] **Step 2: Rewrite `src/menu/pause_menu.cpp`**

```cpp
//
// src/menu/pause_menu.cpp
//
// Bare Metal Sega Genesis
// See pause_menu.h.
//

#include "pause_menu.h"
#include "menu_state.h"             // menu_next_enabled
#include "../input/joypad_map.h"    // GP_UP, GP_DOWN, GP_START, GP_B
#include <circle/timer.h>

#define NUM_ENTRIES 6
#define NUM_SLOTS   4

static const char *const LABELS[NUM_ENTRIES] =
{
    "Resume", "Save State", "Load State", "Reset Game", "Settings",
    "Return to ROM Browser"
};
static const bool ENABLED[NUM_ENTRIES] = { true, true, true, true, false, true };

// RGB565 colours.
static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 GREY  = 0x8410;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

PauseMenu::PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                     SaveState *pSaveState)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSaveState(pSaveState)
{
}

void PauseMenu::Render(int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3;
    int boxY = ch * 2;
    int boxW = cw * 26;
    int boxH = ch * (NUM_ENTRIES + 3);

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "PAUSED", WHITE, BOX);

    for (int i = 0; i < NUM_ENTRIES; i++)
    {
        int ty = boxY + ch * (i + 3);
        bool sel = (i == selected);
        u16 fg = sel ? SELFG : (ENABLED[i] ? WHITE : GREY);
        u16 bg = sel ? SELBG : BOX;
        if (sel)
        {
            m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        }
        m_pCanvas->DrawText(boxX + cw,     ty, sel ? ">" : " ", fg, bg);
        m_pCanvas->DrawText(boxX + cw * 3, ty, LABELS[i],       fg, bg);
    }
}

void PauseMenu::Message(const char *text)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    m_pCanvas->FillRect(cw * 3, ch * 2, cw * 26, ch * 3, BOX);
    m_pCanvas->DrawText(cw * 4, ch * 3, text, WHITE, BOX);
    CTimer::SimpleMsDelay(1200);
}

int PauseMenu::PickSlot(bool forLoad)
{
    bool occupied[NUM_SLOTS];
    bool selable[NUM_SLOTS];
    for (int i = 0; i < NUM_SLOTS; i++)
    {
        occupied[i] = m_pSaveState->Occupied(i + 1);
        selable[i]  = forLoad ? occupied[i] : true;
    }

    int sel = 0;
    if (forLoad)                          // start on the first occupied slot
    {
        sel = 0;
        for (int i = 0; i < NUM_SLOTS; i++) if (occupied[i]) { sel = i; break; }
    }

    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();

    bool     redraw = true;
    unsigned prev   = m_pGamepad->Buttons();
    for (;;)
    {
        if (redraw)
        {
            m_pCanvas->FillRect(cw * 3, ch * 2, cw * 26, ch * (NUM_SLOTS + 5), BOX);
            m_pCanvas->DrawText(cw * 4, ch * 3, forLoad ? "Load State" : "Save State",
                                WHITE, BOX);
            for (int i = 0; i < NUM_SLOTS; i++)
            {
                int ty = ch * (i + 5);
                bool cur = (i == sel) && selable[i];
                u16 fg = cur ? SELFG : (selable[i] ? WHITE : GREY);
                u16 bg = cur ? SELBG : BOX;
                if (cur) m_pCanvas->FillRect(cw * 4, ty, cw * 24, ch, SELBG);
                char digit[2] = { (char) ('1' + i), '\0' };
                m_pCanvas->DrawText(cw * 5,  ty, "Slot", fg, bg);
                m_pCanvas->DrawText(cw * 10, ty, digit,  fg, bg);
                m_pCanvas->DrawText(cw * 13, ty, occupied[i] ? "Used" : "Empty", fg, bg);
            }
            m_pCanvas->DrawText(cw * 4, ch * (NUM_SLOTS + 6),
                                "Start: select   B: cancel", WHITE, BOX);
            redraw = false;
        }

        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)
        {
            int n = menu_next_enabled(selable, NUM_SLOTS, sel, -1);
            if (n != sel) { sel = n; redraw = true; }
        }
        if (pressed & GP_DOWN)
        {
            int n = menu_next_enabled(selable, NUM_SLOTS, sel, +1);
            if (n != sel) { sel = n; redraw = true; }
        }
        if (pressed & GP_B)
        {
            return 0;                     // cancel
        }
        if (pressed & GP_START)
        {
            if (selable[sel]) return sel + 1;   // 1..4
        }

        CTimer::SimpleMsDelay(16);
    }
}

MenuAction PauseMenu::Run(void)
{
    int selected = 0;
    Render(selected);

    unsigned prev = m_pGamepad->Buttons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)
        {
            selected = menu_next_enabled(ENABLED, NUM_ENTRIES, selected, -1);
            Render(selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = menu_next_enabled(ENABLED, NUM_ENTRIES, selected, +1);
            Render(selected);
        }
        if (pressed & GP_START)
        {
            switch (selected)
            {
            case 0:                       // Resume
                return MenuAction::Resume;

            case 1: {                     // Save State
                int slot = PickSlot(false);
                if (slot != 0)
                {
                    bool ok = m_pSaveState->Save(slot);
                    char msg[24] = "Saved to slot 0";
                    msg[14] = (char) ('0' + slot);
                    Message(ok ? msg : "Save failed.");
                }
                Render(selected);
                prev = m_pGamepad->Buttons();
                break;
            }

            case 2: {                     // Load State
                int slot = PickSlot(true);
                if (slot != 0)
                {
                    if (m_pSaveState->Load(slot)) return MenuAction::Resume;
                    Message("Load failed.");
                }
                Render(selected);
                prev = m_pGamepad->Buttons();
                break;
            }

            case 3:                       // Reset Game
                return MenuAction::Reset;

            case 5:                       // Return to ROM Browser
                return MenuAction::ReturnToBrowser;

            default:
                break;
            }
        }

        CTimer::SimpleMsDelay(16);
    }
}
```

- [ ] **Step 3: Add the `SaveState` include + member in `src/kernel.h`**

Replace:

```cpp
#include "ui/text_canvas.h"
#include "menu/pause_menu.h"
#include "input/gamepad.h"
```

with:

```cpp
#include "ui/text_canvas.h"
#include "menu/save_state.h"
#include "menu/pause_menu.h"
#include "input/gamepad.h"
```

Replace:

```cpp
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	RomMenu            m_RomMenu;    // on-screen ROM browser
	PauseMenu          m_PauseMenu;  // in-emulation overlay menu
```

with:

```cpp
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	SaveState          m_SaveState;  // save/load core state to SD slots
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	RomMenu            m_RomMenu;    // on-screen ROM browser
	PauseMenu          m_PauseMenu;  // in-emulation overlay menu
```

- [ ] **Step 4: Update the constructor init list in `src/kernel.cpp`**

Replace:

```cpp
	m_Storage (),
	m_Canvas (&m_Display),
	m_RomMenu (&m_Canvas, &m_Gamepad, &m_Storage, &m_USBHCI),
	m_PauseMenu (&m_Canvas, &m_Gamepad, &m_USBHCI),
```

with:

```cpp
	m_Storage (),
	m_SaveState (&m_Storage),
	m_Canvas (&m_Display),
	m_RomMenu (&m_Canvas, &m_Gamepad, &m_Storage, &m_USBHCI),
	m_PauseMenu (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_SaveState),
```

- [ ] **Step 5: Set the current game per load in `src/kernel.cpp`**

In `Run()`, replace:

```cpp
		retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
		retro_set_controller_port_device (1, RETRO_DEVICE_NONE);

		// --- Play ---
```

with:

```cpp
		retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
		retro_set_controller_port_device (1, RETRO_DEVICE_NONE);

		m_SaveState.SetGame (romPath);   // save/load target for this game

		// --- Play ---
```

- [ ] **Step 6: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `pause_menu.o`, `kernel.o`, links, exit 0.

- [ ] **Step 7: Run the host suite (regression)**

Run: `make -C test run`
Expected: all suites pass (including `save_path`).

- [ ] **Step 8: Commit**

```bash
git add src/menu/pause_menu.h src/menu/pause_menu.cpp src/kernel.h src/kernel.cpp
git commit -m "Save states: slot picker in PauseMenu + kernel wiring"
```

---

## Hardware verification (after Task 4)

Flash `kernel7.img` with a `/roms` folder + a USB gamepad:

- [ ] Launch a game; Start+Select opens the pause menu; Save State and Load State are no longer greyed.
- [ ] Save State → slot picker shows Slot 1-4 (all Empty initially); pick Slot 2 → "Saved to slot 2".
- [ ] Play on (change game state), then Load State → Slot 2 shows "Used" and is selectable → state restored exactly.
- [ ] Load State picker: Empty slots are greyed and cannot be selected; B cancels.
- [ ] Overwrite Slot 2 with a newer state, Load it → restores the newer state.
- [ ] Power-cycle, boot the same ROM, Load Slot 2 → restores (saves persist on SD).
- [ ] A second game uses its own slots (different `saves/<rom>.stateN` files).

---

## Notes for the implementer

- **Member/init order:** `m_SaveState` is declared after `m_Storage` (takes
  `&m_Storage`) and before `m_PauseMenu` (which takes `&m_SaveState`). The init
  list matches declaration order.
- **`GP_B`** (controller B, `0x2`) is the slot-picker cancel; it comes from
  `src/input/joypad_map.h`, already included by `pause_menu.cpp`.
- **Edge detection / resync:** after `PickSlot`/`Message`, `Run()` resyncs `prev`
  to the current buttons so a held Start doesn't immediately re-trigger.
- **`retro_serialize_size()`** is constant for GPGX; `Save` allocates exactly that
  many bytes. `Load` passes the file's size to `retro_unserialize`, which returns
  false on a size mismatch (handled as "Load failed").
- **No host test for `SaveState`/`PauseMenu`** — they depend on libretro + the
  framebuffer/gamepad; only `state_path` is host-tested. The rest is verified on
  hardware.
```
