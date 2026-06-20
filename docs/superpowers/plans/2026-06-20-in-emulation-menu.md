# In-Emulation Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Start+Select pause menu during gameplay (Resume / Reset / Return to ROM Browser, plus greyed Save/Load/Settings), with all menus rendered on the game framebuffer.

**Architecture:** Add a `TextCanvas` that draws text into `Display`'s RGB565 framebuffer via Circle's `CCharGenerator`. Move `Display.Initialize()` to boot so the ROM browser, gameplay, and pause menu share one framebuffer (no console switching). `RomMenu` is refactored onto `TextCanvas`; the kernel's `Run()` becomes a browse↔play loop with hotkey-driven pause/reset/return.

**Tech Stack:** C++ bare metal, Circle (`CCharGenerator`, `Font12x22`), Genesis-Plus-GX-Wide. Host tests with system `c++` in `test/`.

**Spec:** `docs/superpowers/specs/2026-06-20-in-emulation-menu-design.md`

---

## File structure

| File | Responsibility |
|---|---|
| `src/menu/menu_state.{h,cpp}` (modify) | Add pure `menu_next_enabled`. |
| `src/video/display.h` (modify) | Expose `Buffer/Pitch/Width/Height`. |
| `src/ui/text_canvas.{h,cpp}` (new) | Draw text + rects into `Display`'s framebuffer. |
| `src/menu/pause_menu.{h,cpp}` (new) | `PauseMenu` overlay; `Run()->MenuAction`. |
| `src/menu/rom_menu.{h,cpp}` (modify) | Render via `TextCanvas` instead of `CScreenDevice`. |
| `src/kernel.{h,cpp}` (modify) | Boot-time `Display.Initialize()`; browse↔play loop; hotkey + pause dispatch. |
| `Makefile` (modify) | Add `src/ui/text_canvas.o`, `src/menu/pause_menu.o`; clean `src/ui`. |
| `test/test_menu_state.cpp` (modify) | Add `menu_next_enabled` cases. |

---

## Task 1: `menu_next_enabled` (pure, host-tested)

**Files:**
- Modify: `src/menu/menu_state.h`, `src/menu/menu_state.cpp`
- Modify: `test/test_menu_state.cpp`

- [ ] **Step 1: Add the failing test cases**

In `test/test_menu_state.cpp`, add before the final `printf`:

```cpp
    // menu_next_enabled: pause-menu layout (Resume, -, -, Reset, -, Return).
    const bool en[6] = {true, false, false, true, false, true};
    assert(menu_next_enabled(en, 6, 0, +1) == 3);   // skip 1,2
    assert(menu_next_enabled(en, 6, 3, +1) == 5);   // skip 4
    assert(menu_next_enabled(en, 6, 5, +1) == 5);   // none after -> stay
    assert(menu_next_enabled(en, 6, 5, -1) == 3);
    assert(menu_next_enabled(en, 6, 3, -1) == 0);
    assert(menu_next_enabled(en, 6, 0, -1) == 0);   // none before -> stay
    const bool none[2] = {false, false};
    assert(menu_next_enabled(none, 2, 0, +1) == 0); // all disabled -> stay
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -C test test_menu_state`
Expected: FAIL — `undefined reference to menu_next_enabled`.

- [ ] **Step 3: Declare it in `src/menu/menu_state.h`**

Add after the `menu_move` declaration:

```cpp
// Starting from `from`, return the next index in direction `dir` (+1 or -1)
// whose enabled[] is true. No wrap: if there is no enabled entry that way (or
// none at all), returns `from`.
int menu_next_enabled(const bool *enabled, int count, int from, int dir);
```

- [ ] **Step 4: Implement it in `src/menu/menu_state.cpp`**

Append:

```cpp
int menu_next_enabled(const bool *enabled, int count, int from, int dir)
{
    if (count <= 0) return from;
    int step = (dir >= 0) ? 1 : -1;
    for (int i = from + step; i >= 0 && i < count; i += step)
    {
        if (enabled[i]) return i;
    }
    return from;
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_menu_state && ./test/test_menu_state`
Expected: `All menu_state tests passed`

- [ ] **Step 6: Commit**

```bash
git add src/menu/menu_state.h src/menu/menu_state.cpp test/test_menu_state.cpp
git commit -m "In-emu menu: host-tested menu_next_enabled"
```

---

## Task 2: Display framebuffer getters

**Files:**
- Modify: `src/video/display.h`

- [ ] **Step 1: Add getters to the `Display` class**

In `src/video/display.h`, add these public methods right after `void Blit(...)`:

```cpp
    // Framebuffer accessors for on-screen UI (TextCanvas draws into this).
    u16     *Buffer(void) const { return m_pBuffer; }
    unsigned Pitch (void) const { return m_Pitch; }   // bytes per row
    unsigned Width (void) const { return m_FbW; }     // pixels
    unsigned Height(void) const { return m_FbH; }     // pixels
```

- [ ] **Step 2: Cross-build to verify it compiles**

Run: `make`
Expected: builds, ends `WC  kernel7.img => <size>`, exit 0. (Header-only change; no behaviour change.)

- [ ] **Step 3: Commit**

```bash
git add src/video/display.h
git commit -m "In-emu menu: expose Display framebuffer getters"
```

---

## Task 3: TextCanvas (framebuffer text renderer)

No host test (renders into a live framebuffer); verified by cross build now and on hardware later.

**Files:**
- Create: `src/ui/text_canvas.h`, `src/ui/text_canvas.cpp`
- Modify: `Makefile`

- [ ] **Step 1: Write `src/ui/text_canvas.h`**

```cpp
//
// src/ui/text_canvas.h
//
// Bare Metal Sega Genesis
// Draws text and filled rectangles into the Display's RGB565 framebuffer using
// Circle's CCharGenerator font. The single primitive for on-screen UI.
//

#ifndef _ui_text_canvas_h
#define _ui_text_canvas_h

#include <circle/chargenerator.h>
#include <circle/types.h>
#include "../video/display.h"

class TextCanvas
{
public:
    TextCanvas(Display *pDisplay);

    unsigned CharW(void) const;   // glyph width in pixels
    unsigned CharH(void) const;   // glyph height in pixels
    unsigned Cols(void) const;    // Width()  / CharW()
    unsigned Rows(void) const;    // Height() / CharH()

    void Clear(u16 color);
    void FillRect(int x, int y, int w, int h, u16 color);          // pixels, clipped
    void DrawText(int x, int y, const char *s, u16 fg, u16 bg);    // glyph px = fg, cell = bg

private:
    Display       *m_pDisplay;
    CCharGenerator m_Font;
};

#endif
```

- [ ] **Step 2: Write `src/ui/text_canvas.cpp`**

```cpp
//
// src/ui/text_canvas.cpp
//
// Bare Metal Sega Genesis
// See text_canvas.h.
//

#include "text_canvas.h"
#include <circle/font.h>

TextCanvas::TextCanvas(Display *pDisplay)
:   m_pDisplay(pDisplay), m_Font(Font12x22)
{
}

unsigned TextCanvas::CharW(void) const { return m_Font.GetCharWidth(); }
unsigned TextCanvas::CharH(void) const { return m_Font.GetCharHeight(); }

unsigned TextCanvas::Cols(void) const
{
    unsigned cw = CharW();
    return cw ? m_pDisplay->Width() / cw : 0;
}

unsigned TextCanvas::Rows(void) const
{
    unsigned ch = CharH();
    return ch ? m_pDisplay->Height() / ch : 0;
}

void TextCanvas::FillRect(int x, int y, int w, int h, u16 color)
{
    u16 *buf = m_pDisplay->Buffer();
    if (buf == 0) return;
    unsigned pitchPx = m_pDisplay->Pitch() / 2;
    int fbw = (int) m_pDisplay->Width();
    int fbh = (int) m_pDisplay->Height();
    for (int yy = y; yy < y + h; yy++)
    {
        if (yy < 0 || yy >= fbh) continue;
        for (int xx = x; xx < x + w; xx++)
        {
            if (xx < 0 || xx >= fbw) continue;
            buf[(unsigned) yy * pitchPx + (unsigned) xx] = color;
        }
    }
}

void TextCanvas::Clear(u16 color)
{
    FillRect(0, 0, (int) m_pDisplay->Width(), (int) m_pDisplay->Height(), color);
}

void TextCanvas::DrawText(int x, int y, const char *s, u16 fg, u16 bg)
{
    u16 *buf = m_pDisplay->Buffer();
    if (buf == 0 || s == 0) return;
    unsigned pitchPx = m_pDisplay->Pitch() / 2;
    int fbw = (int) m_pDisplay->Width();
    int fbh = (int) m_pDisplay->Height();
    unsigned cw = CharW(), ch = CharH();

    int penx = x;
    for (unsigned i = 0; s[i] != '\0'; i++)
    {
        char c = s[i];
        for (unsigned py = 0; py < ch; py++)
        {
            int yy = y + (int) py;
            if (yy < 0 || yy >= fbh) continue;
            for (unsigned px = 0; px < cw; px++)
            {
                int xx = penx + (int) px;
                if (xx < 0 || xx >= fbw) continue;
                boolean on = m_Font.GetPixel(c, px, py);
                buf[(unsigned) yy * pitchPx + (unsigned) xx] = on ? fg : bg;
            }
        }
        penx += (int) cw;
    }
}
```

- [ ] **Step 3: Add `text_canvas.o` to `OBJS` in `Makefile`**

Replace:

```make
       src/menu/menu_path.o \
       src/menu/rom_menu.o
```

with:

```make
       src/menu/menu_path.o \
       src/menu/rom_menu.o \
       src/ui/text_canvas.o
```

- [ ] **Step 4: Add `src/ui` to `EXTRACLEAN` in `Makefile`**

Replace:

```make
             src/menu/*.o src/menu/*.d \
```

with:

```make
             src/menu/*.o src/menu/*.d \
             src/ui/*.o src/ui/*.d \
```

- [ ] **Step 5: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `src/ui/text_canvas.o`, links, exit 0. (Unused so far.)

- [ ] **Step 6: Commit**

```bash
git add src/ui/text_canvas.h src/ui/text_canvas.cpp Makefile
git commit -m "In-emu menu: TextCanvas framebuffer text renderer"
```

---

## Task 4: PauseMenu

No host test (renders + reads gamepad); cross-build verified now, behaviour on hardware. Built but not yet wired into the kernel.

**Files:**
- Create: `src/menu/pause_menu.h`, `src/menu/pause_menu.cpp`
- Modify: `Makefile`

- [ ] **Step 1: Write `src/menu/pause_menu.h`**

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

enum class MenuAction { Resume, Reset, ReturnToBrowser };

class PauseMenu
{
public:
    PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI);
    MenuAction Run(void);     // draws over the last frame; returns on confirm

private:
    void Render(int selected);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
};

#endif
```

- [ ] **Step 2: Write `src/menu/pause_menu.cpp`**

```cpp
//
// src/menu/pause_menu.cpp
//
// Bare Metal Sega Genesis
// See pause_menu.h.
//

#include "pause_menu.h"
#include "menu_state.h"             // menu_next_enabled
#include "../input/joypad_map.h"    // GP_UP, GP_DOWN, GP_START
#include <circle/timer.h>

#define NUM_ENTRIES 6

static const char *const LABELS[NUM_ENTRIES] =
{
    "Resume", "Save State", "Load State", "Reset Game", "Settings",
    "Return to ROM Browser"
};
static const bool ENABLED[NUM_ENTRIES] = { true, false, false, true, false, true };
static const MenuAction ACTIONS[NUM_ENTRIES] =
{
    MenuAction::Resume, MenuAction::Resume, MenuAction::Resume,
    MenuAction::Reset,  MenuAction::Resume, MenuAction::ReturnToBrowser
};   // disabled entries' actions are never reached

// RGB565 colours.
static const u16 BOX  = 0x0008;   // near-black box
static const u16 WHITE = 0xFFFF;
static const u16 GREY  = 0x8410;
static const u16 SELFG = 0x0000;  // black text on the selection bar
static const u16 SELBG = 0x07FF;  // cyan selection bar

PauseMenu::PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI)
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

MenuAction PauseMenu::Run(void)
{
    int selected = 0;   // Resume (enabled)
    Render(selected);

    // Require the hotkey buttons to be released before Start counts as confirm,
    // so opening the menu (Start+Select) does not instantly select Resume.
    unsigned prev = m_pGamepad->Buttons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now = m_pGamepad->Buttons();
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
            return ACTIONS[selected];
        }

        CTimer::SimpleMsDelay(16);
    }
}
```

- [ ] **Step 3: Add `pause_menu.o` to `OBJS` in `Makefile`**

Replace:

```make
       src/menu/rom_menu.o \
       src/ui/text_canvas.o
```

with:

```make
       src/menu/rom_menu.o \
       src/menu/pause_menu.o \
       src/ui/text_canvas.o
```

- [ ] **Step 4: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `src/menu/pause_menu.o`, links, exit 0.

- [ ] **Step 5: Commit**

```bash
git add src/menu/pause_menu.h src/menu/pause_menu.cpp Makefile
git commit -m "In-emu menu: PauseMenu overlay (Resume/Reset/Return + greyed entries)"
```

---

## Task 5: RomMenu on TextCanvas + Display at boot

Refactors the ROM browser to render on the game framebuffer and moves
`Display.Initialize()` to boot. After this task the browser works on `m_Display`
(single-game flow unchanged); the browse↔play loop comes in Task 6.

**Files:**
- Modify: `src/menu/rom_menu.h`, `src/menu/rom_menu.cpp`
- Modify: `src/kernel.h`, `src/kernel.cpp`

- [ ] **Step 1: Rewrite `src/menu/rom_menu.h`**

```cpp
//
// src/menu/rom_menu.h
//
// Bare Metal Sega Genesis
// On-screen ROM browser: scans SD:/roms, navigates subfolders with the gamepad,
// returns the selected ROM's full path. Renders on the game framebuffer.
//

#ifndef _menu_rom_menu_h
#define _menu_rom_menu_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../storage/storage.h"
#include "../input/gamepad.h"
#include "menu_state.h"

#define ROM_MENU_MAX_ENTRIES 512

class RomMenu
{
public:
    RomMenu(TextCanvas *pCanvas, Gamepad *pGamepad,
            Storage *pStorage, CUSBHCIDevice *pUSBHCI);

    // Browse from SD:/roms. On launch, writes the selected ROM's full path
    // ("SD:/roms/.../Game.md") to outPath and returns true. Returns false only
    // when the root has no entries at all (caller halts).
    bool Run(char *outPath, unsigned outSize);

private:
    void Scan(void);
    void Render(const MenuState &s);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    Storage       *m_pStorage;
    CUSBHCIDevice *m_pUSBHCI;

    char  m_path[300];
    Entry m_entries[ROM_MENU_MAX_ENTRIES];
    int   m_count;
};

#endif
```

- [ ] **Step 2: Rewrite `src/menu/rom_menu.cpp`**

```cpp
//
// src/menu/rom_menu.cpp
//
// Bare Metal Sega Genesis
// See rom_menu.h.
//

#include "rom_menu.h"
#include "menu_path.h"
#include "../input/joypad_map.h"   // GP_UP, GP_DOWN, GP_START
#include <circle/timer.h>
#include <circle/util.h>           // strcpy, strcmp

static const char ROOT[] = "SD:/roms";

static const u16 WHITE = 0xFFFF;
static const u16 BLACK = 0x0000;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

RomMenu::RomMenu(TextCanvas *pCanvas, Gamepad *pGamepad,
                 Storage *pStorage, CUSBHCIDevice *pUSBHCI)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad),
    m_pStorage(pStorage), m_pUSBHCI(pUSBHCI), m_count(0)
{
    m_path[0] = '\0';
}

void RomMenu::Scan(void)
{
    m_count = 0;
    if (strcmp(m_path, ROOT) != 0)
    {
        m_entries[0].name[0] = '.';
        m_entries[0].name[1] = '.';
        m_entries[0].name[2] = '\0';
        m_entries[0].is_dir = true;
        m_count = 1;
    }
    int got = m_pStorage->ListDir(m_path, &m_entries[m_count],
                                  ROM_MENU_MAX_ENTRIES - m_count);
    if (got > 0) m_count += got;
}

void RomMenu::Render(const MenuState &s)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int rowW = (int) (m_pCanvas->Cols()) * cw;

    m_pCanvas->Clear(BLACK);
    m_pCanvas->DrawText(cw, 0,          "ROMs:", WHITE, BLACK);
    m_pCanvas->DrawText(cw * 7, 0,      m_path,  WHITE, BLACK);

    for (int i = 0; i < s.visible_rows; i++)
    {
        int idx = s.top + i;
        if (idx >= s.count) break;
        int ty = ch * (i + 2);
        bool sel = (idx == s.selected);
        u16 fg = sel ? SELFG : WHITE;
        u16 bg = sel ? SELBG : BLACK;
        if (sel) m_pCanvas->FillRect(0, ty, rowW, ch, SELBG);

        const Entry &e = m_entries[idx];
        m_pCanvas->DrawText(cw, ty, sel ? ">" : " ", fg, bg);
        if (e.is_dir)
        {
            m_pCanvas->DrawText(cw * 3,     ty, "[",     fg, bg);
            m_pCanvas->DrawText(cw * 4,     ty, e.name,  fg, bg);
        }
        else
        {
            m_pCanvas->DrawText(cw * 3,     ty, e.name,  fg, bg);
        }
    }

    int fy = ch * (s.visible_rows + 3);
    m_pCanvas->DrawText(cw, fy, "Up/Down: move   Start: open/launch", WHITE, BLACK);
}

bool RomMenu::Run(char *outPath, unsigned outSize)
{
    strcpy(m_path, ROOT);
    Scan();
    if (m_count == 0)
    {
        m_pCanvas->Clear(BLACK);
        m_pCanvas->DrawText(40, 40, "No ROMs found.", WHITE, BLACK);
        m_pCanvas->DrawText(40, 40 + (int) m_pCanvas->CharH(),
                            "Place .md/.bin/.gen files in /roms", WHITE, BLACK);
        return false;
    }

    int visible = (int) m_pCanvas->Rows() - 5;   // header(2) + footer(2) + margin
    if (visible < 1) visible = 1;

    MenuState s = { m_count, 0, 0, visible };
    Render(s);

    unsigned prev = 0;
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)   { menu_move(&s, -1); Render(s); }
        if (pressed & GP_DOWN) { menu_move(&s, +1); Render(s); }

        if (pressed & GP_START)
        {
            const Entry &e = m_entries[s.selected];
            if (e.is_dir && strcmp(e.name, "..") == 0)
            {
                path_parent(m_path, ROOT, m_path, sizeof m_path);
                Scan(); s.count = m_count; s.selected = 0; s.top = 0; Render(s);
            }
            else if (e.is_dir)
            {
                path_join(m_path, e.name, m_path, sizeof m_path);
                Scan(); s.count = m_count; s.selected = 0; s.top = 0; Render(s);
            }
            else
            {
                path_join(m_path, e.name, outPath, outSize);
                return true;
            }
        }

        CTimer::SimpleMsDelay(16);
    }
}
```

- [ ] **Step 3: Update `src/kernel.h` — include + members**

Replace:

```cpp
#include "video/display.h"
#include "input/gamepad.h"
```

with:

```cpp
#include "video/display.h"
#include "ui/text_canvas.h"
#include "input/gamepad.h"
```

Replace:

```cpp
	Gamepad            m_Gamepad;    // USB controller input (M7)
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	RomMenu            m_RomMenu;    // on-screen ROM browser
```

with:

```cpp
	Gamepad            m_Gamepad;    // USB controller input (M7)
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	RomMenu            m_RomMenu;    // on-screen ROM browser
```

- [ ] **Step 4: Update the constructor init list in `src/kernel.cpp`**

Replace:

```cpp
	m_Storage (),
	m_RomMenu (&m_Screen, &m_Gamepad, &m_Storage, &m_USBHCI),
```

with:

```cpp
	m_Storage (),
	m_Canvas (&m_Display),
	m_RomMenu (&m_Canvas, &m_Gamepad, &m_Storage, &m_USBHCI),
```

- [ ] **Step 5: Initialize the Display at boot in `CKernel::Initialize()`**

In `src/kernel.cpp`, replace:

```cpp
	if (bOK)
	{
		// USB gamepads enumerate asynchronously via plug-and-play; the pad is
		// acquired later in the frame loop (UpdatePlugAndPlay + Gamepad::Poll).
		m_Logger.Write (FromKernel, LogNotice, "Input: USB gamepad (plug-and-play)");
	}

	return bOK;
```

with:

```cpp
	if (bOK)
	{
		m_Logger.Write (FromKernel, LogNotice, "Initialising video");
		bOK = m_Display.Initialize ();
		if (!bOK)
		{
			m_Logger.Write (FromKernel, LogPanic, "Display init failed");
		}
	}

	if (bOK)
	{
		// USB gamepads enumerate asynchronously via plug-and-play; the pad is
		// acquired later in the frame loop (UpdatePlugAndPlay + Gamepad::Poll).
		m_Logger.Write (FromKernel, LogNotice, "Input: USB gamepad (plug-and-play)");
	}

	return bOK;
```

- [ ] **Step 6: Remove the mid-`Run()` Display init in `src/kernel.cpp`**

Delete this block (the Display is now initialized at boot):

```cpp
	// Hand the screen over from the menu console to the game framebuffer.
	if (!m_Display.Initialize ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Display init failed");
		return ShutdownHalt;
	}

```

- [ ] **Step 7: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `rom_menu.o`, `kernel.o`, links, exit 0.

- [ ] **Step 8: Run the host suite (regression)**

Run: `make -C test run`
Expected: all suites pass (RomMenu logic unchanged).

- [ ] **Step 9: Commit**

```bash
git add src/menu/rom_menu.h src/menu/rom_menu.cpp src/kernel.h src/kernel.cpp
git commit -m "In-emu menu: render ROM browser on the game framebuffer (Display at boot)"
```

---

## Task 6: Kernel browse↔play loop + hotkey + pause dispatch

Restructures `Run()` into an outer browse↔play loop, wires the pause menu, and
makes a failed ROM return to the browser instead of halting.

**Files:**
- Modify: `src/kernel.h`, `src/kernel.cpp`

- [ ] **Step 1: Add the PauseMenu include + member in `src/kernel.h`**

Replace:

```cpp
#include "ui/text_canvas.h"
#include "input/gamepad.h"
```

with:

```cpp
#include "ui/text_canvas.h"
#include "menu/pause_menu.h"
#include "input/gamepad.h"
```

Replace:

```cpp
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	RomMenu            m_RomMenu;    // on-screen ROM browser
```

with:

```cpp
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	RomMenu            m_RomMenu;    // on-screen ROM browser
	PauseMenu          m_PauseMenu;  // in-emulation overlay menu
```

- [ ] **Step 2: Construct the PauseMenu in the init list in `src/kernel.cpp`**

Replace:

```cpp
	m_Canvas (&m_Display),
	m_RomMenu (&m_Canvas, &m_Gamepad, &m_Storage, &m_USBHCI),
```

with:

```cpp
	m_Canvas (&m_Display),
	m_RomMenu (&m_Canvas, &m_Gamepad, &m_Storage, &m_USBHCI),
	m_PauseMenu (&m_Canvas, &m_Gamepad, &m_USBHCI),
```

- [ ] **Step 3: Add the joypad-bit include in `src/kernel.cpp`**

At the top of `src/kernel.cpp`, replace:

```cpp
#include "kernel.h"
```

with:

```cpp
#include "kernel.h"
#include "input/joypad_map.h"   // GP_START, GP_SELECT bits for the menu hotkey
```

- [ ] **Step 4: Replace the whole body of `CKernel::Run()` in `src/kernel.cpp`**

Replace everything from `TShutdownMode CKernel::Run (void)` through its closing
`}` (the current single-game flow) with:

```cpp
TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice,
		"Bare Metal Sega Genesis — build " __DATE__ " " __TIME__);

	if (!m_Storage.Mount ())
	{
		m_Logger.Write (FromKernel, LogPanic, "SD card mount failed");
		return ShutdownHalt;
	}

	// libretro callbacks + core init: once for the whole session.
	retro_set_environment (environment_cb);
	retro_set_video_refresh (video_refresh_cb);
	retro_set_audio_sample (audio_sample_cb);
	retro_set_audio_sample_batch (audio_batch_cb);
	retro_set_input_poll (input_poll_cb);
	retro_set_input_state (input_state_cb);
	retro_init ();

	g_display = &m_Display;
	g_gamepad = &m_Gamepad;

	#define RETRO_DEVICE_MDPAD_6B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
	const unsigned HOTKEY = GP_START | GP_SELECT;

	boolean audioInited = FALSE;

	for (;;)   // browse <-> play
	{
		// --- Browse ---
		char romPath[300];
		if (!m_RomMenu.Run (romPath, sizeof romPath))
		{
			m_Logger.Write (FromKernel, LogPanic, "No ROMs found in /roms");
			return ShutdownHalt;   // RomMenu already drew the message
		}

		if (!m_Storage.ReadFile (romPath, &m_pROMBuffer, &m_nROMSize))
		{
			m_Canvas.Clear (0x0000);
			m_Canvas.DrawText (40, 40, "Failed to read ROM.", 0xF800, 0x0000);
			m_Canvas.DrawText (40, 40 + (int) m_Canvas.CharH (),
				"Returning to browser...", 0xFFFF, 0x0000);
			CTimer::SimpleMsDelay (2000);
			continue;
		}

		g_rom_data = m_pROMBuffer;
		g_rom_size = m_nROMSize;

		struct retro_game_info gameInfo;
		gameInfo.path = romPath;
		gameInfo.data = m_pROMBuffer;
		gameInfo.size = m_nROMSize;
		gameInfo.meta = "";

		if (!retro_load_game (&gameInfo))
		{
			delete[] m_pROMBuffer;
			m_pROMBuffer = 0;
			m_Canvas.Clear (0x0000);
			m_Canvas.DrawText (40, 40, "Failed to load ROM.", 0xF800, 0x0000);
			m_Canvas.DrawText (40, 40 + (int) m_Canvas.CharH (),
				"Returning to browser...", 0xFFFF, 0x0000);
			CTimer::SimpleMsDelay (2000);
			continue;
		}

		// Pacing parameters from the core's A/V info.
		struct retro_system_av_info avInfo;
		retro_get_system_av_info (&avInfo);
		unsigned sampleRate     = (unsigned) avInfo.timing.sample_rate;
		double   fps            = (double) avInfo.timing.fps;
		if (fps < 1.0 || fps > 61.0) fps = 60.0;
		unsigned framesPerVideo = sampleRate ? (unsigned) (sampleRate / fps) : 0;
		unsigned target         = framesPerVideo * 2;
		u64      period_us      = (u64) (1000000.0 / fps);

		// Audio: initialise once (Genesis sample rate is constant).
		if (!audioInited && sampleRate > 0 && m_Audio.Initialize (sampleRate))
		{
			audioInited = TRUE;
			g_audio = &m_Audio;
		}
		boolean audioOK = audioInited;

		retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
		retro_set_controller_port_device (1, RETRO_DEVICE_NONE);

		// --- Play ---
		u64      next      = CTimer::GetClockTicks64 ();
		unsigned frame     = 0;
		boolean  ledOn     = FALSE;
		unsigned prevBtns  = 0;
		boolean  toBrowser = FALSE;

		for (;;)
		{
			m_USBHCI.UpdatePlugAndPlay ();
			m_Gamepad.Poll ();
			unsigned now     = m_Gamepad.Buttons ();
			unsigned pressed = now & ~prevBtns;
			prevBtns = now;

			// Hotkey: Start+Select both held, completed this frame.
			if ((pressed & HOTKEY) && (now & HOTKEY) == HOTKEY)
			{
				MenuAction action = m_PauseMenu.Run ();
				if (action == MenuAction::Reset)
				{
					retro_reset ();
				}
				else if (action == MenuAction::ReturnToBrowser)
				{
					toBrowser = TRUE;
				}
				prevBtns = m_Gamepad.Buttons ();          // resync after the menu
				next     = CTimer::GetClockTicks64 ();    // re-baseline pacing
				if (toBrowser) break;
				continue;
			}

			retro_run ();

			next += period_us;
			u64 t = CTimer::GetClockTicks64 ();
			if (next < t) next = t;
			while (CTimer::GetClockTicks64 () < next) { }

			if (audioOK)
			{
				while (m_Audio.QueuedFrames () > target + framesPerVideo) { }
			}

			if (++frame >= 30)
			{
				frame = 0;
				ledOn = !ledOn;
				if (ledOn) m_ActLED.On (); else m_ActLED.Off ();
			}
		}

		// --- Unload and return to the browser ---
		retro_unload_game ();
		delete[] m_pROMBuffer;
		m_pROMBuffer = 0;
	}

	return ShutdownHalt;
}
```

- [ ] **Step 5: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `kernel.o`, links, exit 0.

- [ ] **Step 6: Run the host suite (regression)**

Run: `make -C test run`
Expected: all suites pass.

- [ ] **Step 7: Commit**

```bash
git add src/kernel.h src/kernel.cpp
git commit -m "In-emu menu: browse<->play loop, Start+Select pause menu, reset/return"
```

---

## Hardware verification (after Task 6)

Flash `kernel7.img` with a `/roms` folder + a USB gamepad:

- [ ] ROM browser appears on the TV (now on the game framebuffer) and launches a game.
- [ ] During gameplay, **Start + Select** opens the pause menu over the frozen frame; audio pauses.
- [ ] D-pad navigation skips the greyed Save State / Load State / Settings; the cursor only lands on Resume / Reset Game / Return to ROM Browser.
- [ ] **Resume** continues the game exactly where it paused.
- [ ] **Reset Game** soft-resets the running game.
- [ ] **Return to ROM Browser** shows the browser again; selecting another ROM loads and runs it.
- [ ] Repeated browse↔play cycles stay stable (no leak/crash/garbled audio).
- [ ] A bad/corrupt ROM shows the error and returns to the browser (no hang).

---

## Notes for the implementer

- **Member/init order:** `m_Canvas` is declared after `m_Display` (it takes `&m_Display`); `m_RomMenu` and `m_PauseMenu` after `m_Canvas`. The constructor init list lists them in declaration order to avoid `-Wreorder`.
- **Hotkey edge logic:** `pressed = now & ~prevBtns`; the combo fires when both `HOTKEY` bits are held and at least one was newly pressed this frame. `PauseMenu::Run()` re-reads `Buttons()` into its own `prev` so the held Start/Select don't immediately confirm; the kernel resyncs `prevBtns` after the menu returns.
- **Pausing:** while the pause menu is open the kernel does not call `retro_run`, so the last frame stays on screen (the overlay draws on top) and audio drains to silence; both resume cleanly.
- **Colours are RGB565** `u16`: white `0xFFFF`, black `0x0000`, red `0xF800`, cyan `0x07FF`, grey `0x8410`.
- **`Font12x22`** gives ~`Width/12` columns by `Height/22` rows on the native display mode — plenty for the menus.
```
