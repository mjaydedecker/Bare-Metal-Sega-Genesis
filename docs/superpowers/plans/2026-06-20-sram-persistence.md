# SRAM Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist battery-backed cartridge SRAM to `SD:/saves/<rom>.srm` — loaded at ROM load, auto-saved periodically (when changed) and on return to the browser.

**Architecture:** A pure `sram_path` helper (host-tested) and a dedicated `Sram` manager that wraps `retro_get_memory_data/size(RETRO_MEMORY_SAVE_RAM)` + `Storage`, with a dirty-check snapshot and a ~10 s timer. The kernel loads SRAM after `retro_load_game`, ticks it each frame, and saves before `retro_unload_game`.

**Tech Stack:** C++ bare metal, Circle, ChaN FatFS, Genesis-Plus-GX-Wide libretro. Host tests with system `c++` in `test/`.

**Spec:** `docs/superpowers/specs/2026-06-20-sram-persistence-design.md`

---

## File structure

| File | Responsibility |
|---|---|
| `src/menu/save_path.{h,cpp}` (modify) | Add pure `sram_path`. |
| `src/menu/sram.{h,cpp}` (new) | `Sram` manager (load/save/auto-save). |
| `src/kernel.{h,cpp}` (modify) | Wire `Sram` into load / play loop / unload. |
| `Makefile` (modify) | Add `src/menu/sram.o`. |
| `test/test_save_path.cpp` (modify) | Add `sram_path` cases. |

---

## Task 1: `sram_path` (pure, host-tested)

**Files:**
- Modify: `src/menu/save_path.h`, `src/menu/save_path.cpp`, `test/test_save_path.cpp`

- [ ] **Step 1: Declare `sram_path` in `src/menu/save_path.h`**

After the `state_path` declaration, add:

```cpp
// out = "SD:/saves/<filename>.srm", where <filename> is romPath with its
// directory stripped (extension kept). Bounded by out_size.
void sram_path(const char *romPath, char *out, unsigned out_size);
```

- [ ] **Step 2: Add failing test cases to `test/test_save_path.cpp`**

Before the final `printf`, add:

```cpp
    state_path("Game.bin", 4, out, sizeof out);   // (existing case kept above)

    sram_path("SD:/roms/Sonic.md", out, sizeof out);
    assert(strcmp(out, "SD:/saves/Sonic.md.srm") == 0);

    sram_path("SD:/roms/Genesis/Phantasy Star.md", out, sizeof out);
    assert(strcmp(out, "SD:/saves/Phantasy Star.md.srm") == 0);

    sram_path("Game.bin", out, sizeof out);        // no directory
    assert(strcmp(out, "SD:/saves/Game.bin.srm") == 0);
```

(The line `state_path("Game.bin", 4, ...)` already exists; the snippet just shows
where to add the `sram_path` asserts — directly after it, before `printf`.)

- [ ] **Step 3: Run the test to verify it fails**

Run: `make -C test test_save_path`
Expected: FAIL — `undefined reference to sram_path`.

- [ ] **Step 4: Implement `sram_path` in `src/menu/save_path.cpp`**

Append at the end of the file (reuses the existing static `append` helper):

```cpp
void sram_path(const char *romPath, char *out, unsigned out_size)
{
    if (out_size == 0) return;

    const char *base = romPath;            // basename: after the last '/'
    for (unsigned i = 0; romPath[i] != '\0'; i++)
    {
        if (romPath[i] == '/') base = &romPath[i + 1];
    }

    unsigned pos = 0;
    append(out, out_size, &pos, "SD:/saves/");
    append(out, out_size, &pos, base);
    append(out, out_size, &pos, ".srm");
    out[pos] = '\0';
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_save_path && ./test/test_save_path`
Expected: `All save_path tests passed`

- [ ] **Step 6: Commit**

```bash
git add src/menu/save_path.h src/menu/save_path.cpp test/test_save_path.cpp
git commit -m "SRAM: host-tested sram_path"
```

---

## Task 2: Sram manager

Built and linked but not yet wired into the kernel; cross-build verified.

**Files:**
- Create: `src/menu/sram.h`, `src/menu/sram.cpp`
- Modify: `Makefile`

- [ ] **Step 1: Write `src/menu/sram.h`**

```cpp
//
// src/menu/sram.h
//
// Bare Metal Sega Genesis
// Persist battery-backed cartridge SRAM to SD: load at ROM start, auto-save
// periodically (when changed) and on ROM change. No-op for games without SRAM.
//

#ifndef _menu_sram_h
#define _menu_sram_h

#include <circle/types.h>
#include "../storage/storage.h"

class Sram
{
public:
    Sram(Storage *pStorage);
    ~Sram();

    void SetGame(const char *romPath);  // (re)alloc dirty-check snapshot for this game
    void Load(void);                    // .srm -> core SRAM (if present and file exists)
    void Save(void);                    // core SRAM -> .srm (if present)
    void Tick(void);                    // ~10s dirty-checked auto-save

private:
    bool Present(u8 **ppData, size_t *pSize);   // game has SRAM?

    Storage *m_pStorage;
    char     m_romPath[300];
    u8      *m_pSnapshot;   // last-written copy, for the dirty check
    size_t   m_SnapSize;
    u64      m_LastCheck;   // CTimer ticks (us) of the last Tick action
};

#endif
```

- [ ] **Step 2: Write `src/menu/sram.cpp`**

```cpp
//
// src/menu/sram.cpp
//
// Bare Metal Sega Genesis
// See sram.h.
//

#include "sram.h"
#include "save_path.h"
#include <libretro.h>
#include <circle/timer.h>
#include <circle/util.h>     // memcpy, memcmp
#include <circle/logger.h>

static const char FromSram[]   = "sram";
static const u64  INTERVAL_US   = 10ULL * 1000000ULL;   // 10 s (clock is 1 MHz)

static void copy_str(char *dst, const char *src, unsigned n)
{
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < n; i++) dst[i] = src[i];
    dst[i] = '\0';
}

Sram::Sram(Storage *pStorage)
:   m_pStorage(pStorage), m_pSnapshot(0), m_SnapSize(0), m_LastCheck(0)
{
    m_romPath[0] = '\0';
}

Sram::~Sram()
{
    delete[] m_pSnapshot;
    m_pSnapshot = 0;
}

bool Sram::Present(u8 **ppData, size_t *pSize)
{
    size_t size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    u8    *data = (u8 *) retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (size == 0 || data == 0) return false;
    *ppData = data;
    *pSize  = size;
    return true;
}

void Sram::SetGame(const char *romPath)
{
    copy_str(m_romPath, romPath, sizeof m_romPath);

    delete[] m_pSnapshot;
    m_pSnapshot = 0;
    m_SnapSize  = 0;

    u8    *data;
    size_t size;
    if (Present(&data, &size))
    {
        m_pSnapshot = new u8[size];
        memcpy(m_pSnapshot, data, size);
        m_SnapSize = size;
    }
    m_LastCheck = CTimer::GetClockTicks64();
}

void Sram::Load(void)
{
    u8    *data;
    size_t size;
    if (!Present(&data, &size)) return;

    char path[320];
    sram_path(m_romPath, path, sizeof path);

    u8    *buf  = 0;
    size_t fsize = 0;
    if (!m_pStorage->ReadFile(path, &buf, &fsize)) return;   // absent or error

    if (fsize == size)
    {
        memcpy(data, buf, size);
        if (m_pSnapshot != 0 && m_SnapSize == size) memcpy(m_pSnapshot, buf, size);
        CLogger::Get()->Write(FromSram, LogNotice, "SRAM loaded (%u bytes)",
                              (unsigned) size);
    }
    else
    {
        CLogger::Get()->Write(FromSram, LogWarning,
            "SRAM size mismatch (file %u, core %u); ignored",
            (unsigned) fsize, (unsigned) size);
    }
    delete[] buf;
}

void Sram::Save(void)
{
    u8    *data;
    size_t size;
    if (!Present(&data, &size)) return;

    m_pStorage->MakeDir("SD:/saves");
    char path[320];
    sram_path(m_romPath, path, sizeof path);
    if (m_pStorage->WriteFile(path, data, size))
    {
        if (m_pSnapshot != 0 && m_SnapSize == size) memcpy(m_pSnapshot, data, size);
    }
}

void Sram::Tick(void)
{
    u8    *data;
    size_t size;
    if (!Present(&data, &size)) return;

    u64 now = CTimer::GetClockTicks64();
    if (now - m_LastCheck < INTERVAL_US) return;
    m_LastCheck = now;

    if (m_pSnapshot == 0 || m_SnapSize != size) return;       // safety
    if (memcmp(data, m_pSnapshot, size) != 0)
    {
        m_pStorage->MakeDir("SD:/saves");
        char path[320];
        sram_path(m_romPath, path, sizeof path);
        if (m_pStorage->WriteFile(path, data, size))
        {
            memcpy(m_pSnapshot, data, size);
        }
    }
}
```

- [ ] **Step 3: Add `sram.o` to `OBJS` in `Makefile`**

Replace:

```make
       src/menu/save_path.o \
       src/menu/save_state.o \
       src/ui/text_canvas.o
```

with:

```make
       src/menu/save_path.o \
       src/menu/save_state.o \
       src/menu/sram.o \
       src/ui/text_canvas.o
```

- [ ] **Step 4: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `src/menu/sram.o`, links, exit 0.

- [ ] **Step 5: Commit**

```bash
git add src/menu/sram.h src/menu/sram.cpp Makefile
git commit -m "SRAM: Sram manager (load/save/auto-save battery RAM)"
```

---

## Task 3: Kernel wiring

**Files:**
- Modify: `src/kernel.h`, `src/kernel.cpp`

- [ ] **Step 1: Add the include + member in `src/kernel.h`**

Replace:

```cpp
#include "ui/text_canvas.h"
#include "menu/save_state.h"
#include "menu/pause_menu.h"
#include "input/gamepad.h"
```

with:

```cpp
#include "ui/text_canvas.h"
#include "menu/save_state.h"
#include "menu/sram.h"
#include "menu/pause_menu.h"
#include "input/gamepad.h"
```

Replace:

```cpp
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	SaveState          m_SaveState;  // save/load core state to SD slots
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
```

with:

```cpp
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	SaveState          m_SaveState;  // save/load core state to SD slots
	Sram               m_Sram;       // battery SRAM persistence
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
```

- [ ] **Step 2: Construct `m_Sram` in the init list in `src/kernel.cpp`**

Replace:

```cpp
	m_SaveState (&m_Storage),
	m_Canvas (&m_Display),
```

with:

```cpp
	m_SaveState (&m_Storage),
	m_Sram (&m_Storage),
	m_Canvas (&m_Display),
```

- [ ] **Step 3: Load SRAM after the game loads in `src/kernel.cpp`**

Replace:

```cpp
		m_SaveState.SetGame (romPath);   // save/load target for this game

		// --- Play ---
```

with:

```cpp
		m_SaveState.SetGame (romPath);   // save/load target for this game
		m_Sram.SetGame (romPath);
		m_Sram.Load ();                  // restore battery SRAM if present

		// --- Play ---
```

- [ ] **Step 4: Auto-save SRAM each frame in the play loop in `src/kernel.cpp`**

Replace:

```cpp
			retro_run ();

			next += period_us;
```

with:

```cpp
			retro_run ();
			m_Sram.Tick ();              // periodic dirty-checked SRAM auto-save

			next += period_us;
```

- [ ] **Step 5: Save SRAM on return-to-browser in `src/kernel.cpp`**

Replace:

```cpp
		// --- Unload and return to the browser ---
		retro_unload_game ();
		delete[] m_pROMBuffer;
		m_pROMBuffer = 0;
```

with:

```cpp
		// --- Unload and return to the browser ---
		m_Sram.Save ();                  // flush battery SRAM before unloading
		retro_unload_game ();
		delete[] m_pROMBuffer;
		m_pROMBuffer = 0;
```

- [ ] **Step 6: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `kernel.o`, links, exit 0.

- [ ] **Step 7: Run the host suite (regression)**

Run: `make -C test run`
Expected: all suites pass (including `save_path`).

- [ ] **Step 8: Commit**

```bash
git add src/kernel.h src/kernel.cpp
git commit -m "SRAM: wire battery SRAM load/auto-save/flush into the kernel"
```

---

## Hardware verification (after Task 3)

Flash `kernel7.img` with `/roms` + a USB gamepad:

- [ ] A game with battery SRAM (an RPG): make an in-game save, wait ~10 s, power-cycle, reload the ROM → the in-game save is present.
- [ ] In-game save, then Return to ROM Browser, relaunch the same ROM → save persists.
- [ ] A game without SRAM: no `SD:/saves/<rom>.srm` file is created; no periodic hitch.
- [ ] Deleting/corrupting a `.srm` then loading: game still boots; a fresh save recreates it.

---

## Notes for the implementer

- **Member/init order:** `m_Sram` is declared after `m_SaveState` and before
  `m_Canvas`; it takes `&m_Storage` (declared earlier). The init list matches
  declaration order.
- **Clock units:** `CTimer::GetClockTicks64()` is microseconds (1 MHz), the same
  units the play loop's `period_us` uses; `INTERVAL_US = 10 s`.
- **Dirty check:** the snapshot is seeded in `SetGame` and refreshed on every
  successful write and on a matching `Load`, so a just-loaded or just-saved SRAM
  is never seen as dirty.
- **No-op safety:** every method early-returns when `Present` is false (game has
  no SRAM), so action games create no file and incur no writes.
- **Only `sram_path` is host-tested;** `Sram` depends on libretro + storage and is
  verified on hardware.
```
