# ROM Browser Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On boot, show a scrollable, gamepad-navigable menu of the Genesis ROMs in the SD card's `/roms` folder (with subfolder browsing) and launch the selected one, replacing the hardcoded `GAME.MD` load.

**Architecture:** Pure, host-tested logic modules (extension filter, scroll math, path join/parent) plus a thin Circle-bound `RomMenu` shell that renders on `CScreenDevice` and reads the M7 `Gamepad`. The storage layer swaps from Circle's built-in root-only `CFATFileSystem` to the add-on ChaN FatFS (`f_opendir`/`f_readdir`, long filenames, subdirectories).

**Tech Stack:** C++ (bare metal), Circle framework, ChaN FatFS (`libs/circle/addon/fatfs`), Genesis-Plus-GX-Wide libretro core. Host tests compiled with the system `c++` in `test/`.

**Spec:** `docs/superpowers/specs/2026-06-19-rom-browser-menu-design.md`

---

## File structure

| File | Responsibility |
|---|---|
| `src/menu/rom_filter.{h,cpp}` | Pure `is_rom_filename` (`.md`/`.bin`/`.gen`). |
| `src/menu/menu_state.{h,cpp}` | Pure `MenuState` + `menu_move` (selection clamp + scroll window). |
| `src/menu/menu_path.{h,cpp}` | Pure `path_join` / `path_parent` (floored at root, alias-safe). |
| `src/storage/storage.{h,cpp}` | ChaN-FatFS `Storage`: `Mount`, `ListDir`, `ReadFile`. Retires `sdcard.{h,cpp}`. |
| `src/menu/rom_menu.{h,cpp}` | `RomMenu`: render + gamepad input + directory navigation. |
| `src/kernel.{h,cpp}` | Wire mount → menu → load → emulate; move `Display.Initialize()` after selection. |
| `test/test_rom_filter.cpp`, `test/test_menu_state.cpp`, `test/test_menu_path.cpp` | Host tests. |
| `Makefile`, `test/Makefile` | Build wiring (add-on FatFS swap, new objects, host tests). |

---

## Task 1: ROM filename filter (pure, host-tested)

**Files:**
- Create: `src/menu/rom_filter.h`, `src/menu/rom_filter.cpp`
- Create: `test/test_rom_filter.cpp`
- Modify: `test/Makefile`

- [ ] **Step 1: Write the header**

`src/menu/rom_filter.h`:

```cpp
//
// src/menu/rom_filter.h
//
// Bare Metal Sega Genesis
// Pure predicate: is this filename a Genesis ROM we should list?
//

#ifndef _menu_rom_filter_h
#define _menu_rom_filter_h

// True if name ends (case-insensitively) in ".md", ".bin", or ".gen", with at
// least one character before the extension. A null/empty name is false.
bool is_rom_filename(const char *name);

#endif
```

- [ ] **Step 2: Write the failing test**

`test/test_rom_filter.cpp`:

```cpp
#include "../src/menu/rom_filter.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(is_rom_filename("Sonic.md"));
    assert(is_rom_filename("GAME.BIN"));
    assert(is_rom_filename("x.gen"));
    assert(is_rom_filename("Long Game Name.MD"));   // long name, mixed case
    assert(!is_rom_filename("readme.txt"));
    assert(!is_rom_filename("noext"));
    assert(!is_rom_filename(".md"));                // bare extension, no stem
    assert(!is_rom_filename(""));
    assert(!is_rom_filename(0));

    printf("All rom_filter tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the test target to `test/Makefile`**

Replace:

```make
run: test_blit test_joypad
	./test_blit
	./test_joypad
```

with:

```make
run: test_blit test_joypad test_rom_filter
	./test_blit
	./test_joypad
	./test_rom_filter
```

Add this target after the `test_joypad` target:

```make
test_rom_filter: test_rom_filter.cpp ../src/menu/rom_filter.cpp ../src/menu/rom_filter.h
	$(CXX) $(CXXFLAGS) -o $@ test_rom_filter.cpp ../src/menu/rom_filter.cpp
```

Replace the `clean` rule with:

```make
clean:
	rm -f test_blit test_joypad test_rom_filter test_menu_state test_menu_path
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test test_rom_filter`
Expected: FAIL to link — `undefined reference to is_rom_filename`.

- [ ] **Step 5: Write the implementation**

`src/menu/rom_filter.cpp`:

```cpp
//
// src/menu/rom_filter.cpp
//
// Bare Metal Sega Genesis
// See rom_filter.h.
//

#include "rom_filter.h"
#include <string.h>

static bool ends_with_ci(const char *s, const char *suffix)
{
    size_t ls = strlen(s);
    size_t lf = strlen(suffix);
    if (lf == 0 || ls <= lf)        // need at least one char before the suffix
        return false;
    const char *p = s + (ls - lf);
    for (size_t i = 0; i < lf; i++)
    {
        char a = p[i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return false;
    }
    return true;
}

bool is_rom_filename(const char *name)
{
    if (name == 0) return false;
    return ends_with_ci(name, ".md")
        || ends_with_ci(name, ".bin")
        || ends_with_ci(name, ".gen");
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test test_rom_filter && ./test/test_rom_filter`
Expected: `All rom_filter tests passed`

- [ ] **Step 7: Commit**

```bash
git add src/menu/rom_filter.h src/menu/rom_filter.cpp test/test_rom_filter.cpp test/Makefile
git commit -m "ROM menu: host-tested is_rom_filename"
```

---

## Task 2: Menu scroll/selection state (pure, host-tested)

**Files:**
- Create: `src/menu/menu_state.h`, `src/menu/menu_state.cpp`
- Create: `test/test_menu_state.cpp`
- Modify: `test/Makefile`

- [ ] **Step 1: Write the header**

`src/menu/menu_state.h`:

```cpp
//
// src/menu/menu_state.h
//
// Bare Metal Sega Genesis
// Pure list selection + scroll-window math. No Circle deps (host-testable).
//

#ifndef _menu_menu_state_h
#define _menu_menu_state_h

struct MenuState
{
    int count;         // number of entries
    int selected;      // 0..count-1
    int top;           // index of the first visible row
    int visible_rows;  // rows that fit on screen
};

// Move selection by delta, clamp to [0,count-1], and scroll `top` so the
// selection stays within [top, top+visible_rows-1]. Safe for count <= 0.
void menu_move(MenuState *s, int delta);

#endif
```

- [ ] **Step 2: Write the failing test**

`test/test_menu_state.cpp`:

```cpp
#include "../src/menu/menu_state.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // 5 entries, window of 3 rows (indices 0,1,2 visible initially).
    MenuState a = {5, 0, 0, 3};
    menu_move(&a, -1); assert(a.selected == 0 && a.top == 0);   // clamp at top
    menu_move(&a, +1); assert(a.selected == 1 && a.top == 0);
    menu_move(&a, +1); assert(a.selected == 2 && a.top == 0);
    menu_move(&a, +1); assert(a.selected == 3 && a.top == 1);   // scroll down
    menu_move(&a, +1); assert(a.selected == 4 && a.top == 2);
    menu_move(&a, +1); assert(a.selected == 4 && a.top == 2);   // clamp at bottom
    menu_move(&a, -3); assert(a.selected == 1 && a.top == 1);   // scroll up
    menu_move(&a, -1); assert(a.selected == 0 && a.top == 0);

    // Fewer entries than the window: top stays 0.
    MenuState b = {2, 0, 0, 10};
    menu_move(&b, +1); assert(b.selected == 1 && b.top == 0);

    // Empty list: nothing moves, no out-of-range.
    MenuState c = {0, 0, 0, 5};
    menu_move(&c, +1); assert(c.selected == 0 && c.top == 0);

    printf("All menu_state tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the test target to `test/Makefile`**

Update the `run` target to:

```make
run: test_blit test_joypad test_rom_filter test_menu_state
	./test_blit
	./test_joypad
	./test_rom_filter
	./test_menu_state
```

Add this target after `test_rom_filter`:

```make
test_menu_state: test_menu_state.cpp ../src/menu/menu_state.cpp ../src/menu/menu_state.h
	$(CXX) $(CXXFLAGS) -o $@ test_menu_state.cpp ../src/menu/menu_state.cpp
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test test_menu_state`
Expected: FAIL to link — `undefined reference to menu_move`.

- [ ] **Step 5: Write the implementation**

`src/menu/menu_state.cpp`:

```cpp
//
// src/menu/menu_state.cpp
//
// Bare Metal Sega Genesis
// See menu_state.h.
//

#include "menu_state.h"

void menu_move(MenuState *s, int delta)
{
    if (s == 0) return;
    if (s->count <= 0) { s->selected = 0; s->top = 0; return; }

    int sel = s->selected + delta;
    if (sel < 0) sel = 0;
    if (sel > s->count - 1) sel = s->count - 1;
    s->selected = sel;

    int rows = s->visible_rows > 0 ? s->visible_rows : 1;
    if (s->selected < s->top)
        s->top = s->selected;                       // scrolled above the window
    else if (s->selected > s->top + rows - 1)
        s->top = s->selected - rows + 1;            // scrolled below the window

    if (s->top < 0) s->top = 0;
    int maxTop = s->count - rows;
    if (maxTop < 0) maxTop = 0;
    if (s->top > maxTop) s->top = maxTop;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test test_menu_state && ./test/test_menu_state`
Expected: `All menu_state tests passed`

- [ ] **Step 7: Commit**

```bash
git add src/menu/menu_state.h src/menu/menu_state.cpp test/test_menu_state.cpp test/Makefile
git commit -m "ROM menu: host-tested menu scroll/selection state"
```

---

## Task 3: Path join / parent (pure, host-tested)

**Files:**
- Create: `src/menu/menu_path.h`, `src/menu/menu_path.cpp`
- Create: `test/test_menu_path.cpp`
- Modify: `test/Makefile`

- [ ] **Step 1: Write the header**

`src/menu/menu_path.h`:

```cpp
//
// src/menu/menu_path.h
//
// Bare Metal Sega Genesis
// Pure path helpers for the ROM browser. Alias-safe: `out` may be the same
// buffer as `base`/`path`.
//

#ifndef _menu_menu_path_h
#define _menu_menu_path_h

// out = base + "/" + name (bounded by out_size).
void path_join(const char *base, const char *name, char *out, unsigned out_size);

// out = path with its last "/component" removed, but never shorter than `root`
// (calling it at `root` yields `root`). Bounded by out_size.
void path_parent(const char *path, const char *root, char *out, unsigned out_size);

#endif
```

- [ ] **Step 2: Write the failing test**

`test/test_menu_path.cpp`:

```cpp
#include "../src/menu/menu_path.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    char out[300];

    path_join("SD:/roms", "Genesis", out, sizeof out);
    assert(strcmp(out, "SD:/roms/Genesis") == 0);
    path_join("SD:/roms/Genesis", "Sonic the Hedgehog.md", out, sizeof out);
    assert(strcmp(out, "SD:/roms/Genesis/Sonic the Hedgehog.md") == 0);

    path_parent("SD:/roms/Genesis", "SD:/roms", out, sizeof out);
    assert(strcmp(out, "SD:/roms") == 0);
    path_parent("SD:/roms/A/B", "SD:/roms", out, sizeof out);
    assert(strcmp(out, "SD:/roms/A") == 0);
    path_parent("SD:/roms", "SD:/roms", out, sizeof out);   // floored at root
    assert(strcmp(out, "SD:/roms") == 0);

    // Alias-safe: out == input buffer.
    char p[300];
    strcpy(p, "SD:/roms/A");
    path_parent(p, "SD:/roms", p, sizeof p);
    assert(strcmp(p, "SD:/roms") == 0);
    strcpy(p, "SD:/roms");
    path_join(p, "Genesis", p, sizeof p);
    assert(strcmp(p, "SD:/roms/Genesis") == 0);

    printf("All menu_path tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the test target to `test/Makefile`**

Update the `run` target to:

```make
run: test_blit test_joypad test_rom_filter test_menu_state test_menu_path
	./test_blit
	./test_joypad
	./test_rom_filter
	./test_menu_state
	./test_menu_path
```

Add this target after `test_menu_state`:

```make
test_menu_path: test_menu_path.cpp ../src/menu/menu_path.cpp ../src/menu/menu_path.h
	$(CXX) $(CXXFLAGS) -o $@ test_menu_path.cpp ../src/menu/menu_path.cpp
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test test_menu_path`
Expected: FAIL to link — `undefined reference to path_join`.

- [ ] **Step 5: Write the implementation**

`src/menu/menu_path.cpp`:

```cpp
//
// src/menu/menu_path.cpp
//
// Bare Metal Sega Genesis
// See menu_path.h.
//

#include "menu_path.h"
#include <string.h>

static void copy_bounded(char *out, unsigned out_size, const char *src)
{
    if (out_size == 0) return;
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < out_size; i++) out[i] = src[i];
    out[i] = '\0';
}

void path_join(const char *base, const char *name, char *out, unsigned out_size)
{
    char tmp[512];                       // build into a temp so out may alias base
    unsigned n = 0;
    for (unsigned i = 0; base[i] != '\0' && n + 1 < sizeof tmp; i++) tmp[n++] = base[i];
    if (n > 0 && tmp[n - 1] != '/' && n + 1 < sizeof tmp) tmp[n++] = '/';
    for (unsigned i = 0; name[i] != '\0' && n + 1 < sizeof tmp; i++) tmp[n++] = name[i];
    tmp[n] = '\0';
    copy_bounded(out, out_size, tmp);
}

void path_parent(const char *path, const char *root, char *out, unsigned out_size)
{
    char tmp[512];
    copy_bounded(tmp, sizeof tmp, path);

    int slash = -1;
    for (int i = (int) strlen(tmp) - 1; i >= 0; i--)
    {
        if (tmp[i] == '/') { slash = i; break; }
    }
    if (slash > 0)       tmp[slash] = '\0';
    else if (slash == 0) tmp[1]     = '\0';          // keep the leading "/"

    if (strlen(tmp) < strlen(root))                  // floor at the root
        copy_bounded(tmp, sizeof tmp, root);

    copy_bounded(out, out_size, tmp);
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test test_menu_path && ./test/test_menu_path`
Expected: `All menu_path tests passed`

- [ ] **Step 7: Run the full host suite**

Run: `make -C test run`
Expected: all five suites print their "passed" line.

- [ ] **Step 8: Commit**

```bash
git add src/menu/menu_path.h src/menu/menu_path.cpp test/test_menu_path.cpp test/Makefile
git commit -m "ROM menu: host-tested path join/parent"
```

---

## Task 4: Storage over ChaN FatFS + build wiring

No host test (FatFS + EMMC are hardware-bound); verification is a clean cross build. `Storage` is added and linked but not yet used by the kernel, so the existing `SDCard`/`CFATFileSystem` path keeps working — the build stays green.

**Files:**
- Create: `src/storage/storage.h`, `src/storage/storage.cpp`
- Modify: `Makefile`

- [ ] **Step 1: Write `src/storage/storage.h`**

```cpp
//
// src/storage/storage.h
//
// Bare Metal Sega Genesis
// Storage over Circle's add-on ChaN FatFS (long filenames + subdirectories).
// All paths are volume-qualified, e.g. "SD:/roms". Replaces sdcard.{h,cpp}.
//

#ifndef _storage_storage_h
#define _storage_storage_h

#include <circle/types.h>
#include "ff.h"   // ChaN FatFS (include path: libs/circle/addon/fatfs)

struct Entry
{
    char name[256];   // filename only; full path = dir + "/" + name
    bool is_dir;
};

class Storage
{
public:
    Storage(void);

    // Mount the SD card volume ("SD:" -> the emmc1 device). Call once at boot.
    bool Mount(void);

    // List `dir`: subdirectories first (is_dir=true), then ROM files
    // (is_dir=false). Returns the entry count, or -1 if `dir` can't be opened.
    int ListDir(const char *dir, Entry *out, int max);

    // Read an entire file into a new[] buffer. Caller owns *ppBuffer.
    bool ReadFile(const char *path, u8 **ppBuffer, size_t *pSize);

private:
    FATFS m_FS;
    bool  m_bMounted;
};

#endif
```

- [ ] **Step 2: Write `src/storage/storage.cpp`**

```cpp
//
// src/storage/storage.cpp
//
// Bare Metal Sega Genesis
// See storage.h.
//

#include "storage.h"
#include "../menu/rom_filter.h"
#include <circle/logger.h>

static const char FromStorage[] = "storage";

static void copy_name(char *dst, const char *src, unsigned dstsize)
{
    unsigned i = 0;
    for (; src[i] != '\0' && i + 1 < dstsize; i++) dst[i] = src[i];
    dst[i] = '\0';
}

Storage::Storage(void)
:   m_bMounted(false)
{
}

bool Storage::Mount(void)
{
    FRESULT r = f_mount(&m_FS, "SD:", 1);   // 1 = mount immediately
    if (r != FR_OK)
    {
        CLogger::Get()->Write(FromStorage, LogError, "f_mount failed (%d)", (int) r);
        return false;
    }
    m_bMounted = true;
    CLogger::Get()->Write(FromStorage, LogNotice, "SD card mounted (FatFS)");
    return true;
}

int Storage::ListDir(const char *dir, Entry *out, int max)
{
    DIR d;
    if (f_opendir(&d, dir) != FR_OK)
        return -1;

    int n = 0;
    FILINFO fno;

    // Pass 1: subdirectories.
    while (n < max && f_readdir(&d, &fno) == FR_OK && fno.fname[0] != '\0')
    {
        if (fno.fattrib & AM_DIR)
        {
            copy_name(out[n].name, fno.fname, sizeof out[n].name);
            out[n].is_dir = true;
            n++;
        }
    }

    // Pass 2: ROM files. f_readdir(&d, 0) rewinds the directory.
    f_readdir(&d, 0);
    while (n < max && f_readdir(&d, &fno) == FR_OK && fno.fname[0] != '\0')
    {
        if (!(fno.fattrib & AM_DIR) && is_rom_filename(fno.fname))
        {
            copy_name(out[n].name, fno.fname, sizeof out[n].name);
            out[n].is_dir = false;
            n++;
        }
    }

    f_closedir(&d);
    return n;
}

bool Storage::ReadFile(const char *path, u8 **ppBuffer, size_t *pSize)
{
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        CLogger::Get()->Write(FromStorage, LogError, "Cannot open: %s", path);
        return false;
    }

    unsigned size = (unsigned) f_size(&file);
    u8 *buf = new u8[size];

    UINT read = 0;
    FRESULT r = f_read(&file, buf, size, &read);
    f_close(&file);

    if (r != FR_OK || read != size)
    {
        CLogger::Get()->Write(FromStorage, LogError, "Read error: %s", path);
        delete[] buf;
        return false;
    }

    *ppBuffer = buf;
    *pSize    = size;
    return true;
}
```

- [ ] **Step 3: Add the add-on FatFS include path in `Makefile`**

Replace:

```make
EXTRAINCLUDE = \
    -I libs/genesis-plus-gx-wide/libretro/libretro-common/include \
    -I libs/genesis-plus-gx-wide/libretro
```

with:

```make
EXTRAINCLUDE = \
    -I libs/genesis-plus-gx-wide/libretro/libretro-common/include \
    -I libs/genesis-plus-gx-wide/libretro \
    -I libs/circle/addon/fatfs
```

- [ ] **Step 4: Add the new objects to `OBJS` in `Makefile`**

Replace:

```make
       src/input/joypad_map.o \
       src/input/gamepad.o
```

with:

```make
       src/input/joypad_map.o \
       src/input/gamepad.o \
       src/storage/storage.o \
       src/menu/rom_filter.o \
       src/menu/menu_state.o \
       src/menu/menu_path.o
```

- [ ] **Step 5: Link the add-on FatFS library in `Makefile`**

In the `LIBS` list, add the add-on archive (keep the built-in ones for now — `SDCard` still needs them). Replace:

```make
       $(CIRCLEHOME)/lib/sound/libsound.a \
       $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
```

with:

```make
       $(CIRCLEHOME)/lib/sound/libsound.a \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
```

- [ ] **Step 6: Add the add-on FatFS build rule + clean entry in `Makefile`**

After the `$(CIRCLEHOME)/addon/SDCard/libsdcard.a:` rule block, add:

```make
$(CIRCLEHOME)/addon/fatfs/libfatfs.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)
```

Replace the `EXTRACLEAN` first line:

```make
EXTRACLEAN = src/*.o src/*.d src/storage/*.o src/storage/*.d \
```

with:

```make
EXTRACLEAN = src/*.o src/*.d src/storage/*.o src/storage/*.d \
             src/menu/*.o src/menu/*.d \
```

- [ ] **Step 7: Cross-build to verify it compiles and links**

Run: `make`
Expected: builds the add-on FatFS (`AR  libfatfs.a`), compiles `storage.o` and the three menu objects, links, ends with `WC  kernel7.img => <size>`, exit 0.

- [ ] **Step 8: Run the host suite (regression)**

Run: `make -C test run`
Expected: all five suites pass.

- [ ] **Step 9: Commit**

```bash
git add src/storage/storage.h src/storage/storage.cpp Makefile
git commit -m "ROM menu: Storage over add-on ChaN FatFS + build wiring"
```

---

## Task 5: RomMenu (render + input + directory navigation)

No host test (renders on `CScreenDevice`, reads the gamepad); verification is a clean cross build. Added but not yet used by the kernel.

**Files:**
- Create: `src/menu/rom_menu.h`, `src/menu/rom_menu.cpp`
- Modify: `Makefile`

- [ ] **Step 1: Write `src/menu/rom_menu.h`**

```cpp
//
// src/menu/rom_menu.h
//
// Bare Metal Sega Genesis
// On-screen ROM browser: scans SD:/roms, navigates subfolders with the gamepad,
// returns the selected ROM's full path. Renders on the text console.
//

#ifndef _menu_rom_menu_h
#define _menu_rom_menu_h

#include <circle/screen.h>
#include <circle/usb/usbhcidevice.h>
#include "../storage/storage.h"
#include "../input/gamepad.h"
#include "menu_state.h"

#define ROM_MENU_MAX_ENTRIES 512

class RomMenu
{
public:
    RomMenu(CScreenDevice *pScreen, Gamepad *pGamepad,
            Storage *pStorage, CUSBHCIDevice *pUSBHCI);

    // Browse from SD:/roms. On launch, writes the selected ROM's full path
    // ("SD:/roms/.../Game.md") to outPath and returns true. Returns false only
    // when the root has no entries at all (caller halts).
    bool Run(char *outPath, unsigned outSize);

private:
    void Scan(void);                 // ListDir(m_path) -> m_entries (+ ".." when deep)
    void Render(const MenuState &s);

    CScreenDevice *m_pScreen;
    Gamepad       *m_pGamepad;
    Storage       *m_pStorage;
    CUSBHCIDevice *m_pUSBHCI;

    char  m_path[300];
    Entry m_entries[ROM_MENU_MAX_ENTRIES];
    int   m_count;
};

#endif
```

- [ ] **Step 2: Write `src/menu/rom_menu.cpp`**

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
#include <circle/util.h>           // strcpy, strcmp, strlen

static const char ROOT[] = "SD:/roms";

RomMenu::RomMenu(CScreenDevice *pScreen, Gamepad *pGamepad,
                 Storage *pStorage, CUSBHCIDevice *pUSBHCI)
:   m_pScreen(pScreen), m_pGamepad(pGamepad),
    m_pStorage(pStorage), m_pUSBHCI(pUSBHCI), m_count(0)
{
    m_path[0] = '\0';
}

static void put(CScreenDevice *s, const char *p)
{
    s->Write(p, strlen(p));
}

void RomMenu::Scan(void)
{
    m_count = 0;
    if (strcmp(m_path, ROOT) != 0)         // not at root: offer a back entry
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
    put(m_pScreen, "\x1b[H\x1b[J");        // cursor home + clear to end = clear
    put(m_pScreen, "  SEGA GENESIS  -  ");
    put(m_pScreen, m_path);
    put(m_pScreen, "\n  --------------------------------\n\n");

    for (int i = 0; i < s.visible_rows; i++)
    {
        int idx = s.top + i;
        if (idx >= s.count) break;
        put(m_pScreen, idx == s.selected ? " > " : "   ");
        const Entry &e = m_entries[idx];
        if (e.is_dir) { put(m_pScreen, "["); put(m_pScreen, e.name); put(m_pScreen, "]"); }
        else          { put(m_pScreen, e.name); }
        put(m_pScreen, "\n");
    }
    put(m_pScreen, "\n  Up/Down: move   Start: open/launch");
}

bool RomMenu::Run(char *outPath, unsigned outSize)
{
    strcpy(m_path, ROOT);
    Scan();
    if (m_count == 0)
    {
        put(m_pScreen, "\x1b[H\x1b[J\n  No ROMs found.\n"
                       "  Place .md/.bin/.gen files in /roms\n");
        return false;
    }

    int visible = (int) m_pScreen->GetRows() - 6;   // header(3) + footer(2) + margin
    if (visible < 1) visible = 1;

    MenuState s = { m_count, 0, 0, visible };
    Render(s);

    unsigned prev = 0;
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();     // gamepad enumerates here (PnP)
        m_pGamepad->Poll();
        unsigned now = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;     // rising edges only
        prev = now;

        if (pressed & GP_UP)   { menu_move(&s, -1); Render(s); }
        if (pressed & GP_DOWN) { menu_move(&s, +1); Render(s); }

        if (pressed & GP_START)
        {
            const Entry &e = m_entries[s.selected];
            if (e.is_dir && strcmp(e.name, "..") == 0)
            {
                path_parent(m_path, ROOT, m_path, sizeof m_path);
                Scan();
                s.count = m_count; s.selected = 0; s.top = 0;
                Render(s);
            }
            else if (e.is_dir)
            {
                path_join(m_path, e.name, m_path, sizeof m_path);
                Scan();
                s.count = m_count; s.selected = 0; s.top = 0;
                Render(s);
            }
            else
            {
                path_join(m_path, e.name, outPath, outSize);
                return true;
            }
        }

        CTimer::SimpleMsDelay(16);          // ~60 Hz input sampling
    }
}
```

- [ ] **Step 3: Add `rom_menu.o` to `OBJS` in `Makefile`**

Replace:

```make
       src/menu/menu_path.o
```

with:

```make
       src/menu/menu_path.o \
       src/menu/rom_menu.o
```

- [ ] **Step 4: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `src/menu/rom_menu.o`, links, ends with `WC  kernel7.img => <size>`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add src/menu/rom_menu.h src/menu/rom_menu.cpp Makefile
git commit -m "ROM menu: RomMenu render + gamepad nav + subfolder browsing"
```

---

## Task 6: Kernel integration (use the menu; retire SDCard)

Swaps the boot flow to mount via `Storage`, run `RomMenu`, then load+launch the selection; moves `Display.Initialize()` to after selection; removes the built-in `CFATFileSystem`/`SDCard` and the built-in FS libraries.

**Files:**
- Modify: `src/kernel.h`, `src/kernel.cpp`, `Makefile`
- Delete: `src/storage/sdcard.h`, `src/storage/sdcard.cpp`

- [ ] **Step 1: Update `src/kernel.h` includes**

Replace:

```cpp
#include <SDCard/emmc.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/types.h>
#include "storage/sdcard.h"
#include "libretro/environment.h"
```

with:

```cpp
#include <SDCard/emmc.h>
#include <circle/types.h>
#include "storage/storage.h"
#include "menu/rom_menu.h"
#include "libretro/environment.h"
```

- [ ] **Step 2: Update `src/kernel.h` members**

Replace:

```cpp
	CEMMCDevice        m_EMMC;       // SD card block device
	CFATFileSystem     m_FileSystem; // FAT filesystem over EMMC
	AudioDriver        m_Audio;      // HDMI audio output (M6)
	Display            m_Display;    // HDMI video output (M5)
	Gamepad            m_Gamepad;    // USB controller input (M7)

	// Storage wrapper — declared after m_FileSystem and m_DeviceNameService.
	SDCard             m_SDCard;
```

with:

```cpp
	CEMMCDevice        m_EMMC;       // SD card block device
	AudioDriver        m_Audio;      // HDMI audio output (M6)
	Display            m_Display;    // HDMI video output (M5)
	Gamepad            m_Gamepad;    // USB controller input (M7)
	Storage            m_Storage;    // SD card filesystem (ChaN FatFS)
	RomMenu            m_RomMenu;    // on-screen ROM browser
```

- [ ] **Step 3: Update the `CKernel` constructor init list in `src/kernel.cpp`**

Replace:

```cpp
	m_Audio (&m_Interrupt),
	m_Gamepad (&m_DeviceNameService),
	m_SDCard (m_FileSystem, m_DeviceNameService),
	m_pROMBuffer (0),
```

with:

```cpp
	m_Audio (&m_Interrupt),
	m_Gamepad (&m_DeviceNameService),
	m_Storage (),
	m_RomMenu (&m_Screen, &m_Gamepad, &m_Storage, &m_USBHCI),
	m_pROMBuffer (0),
```

- [ ] **Step 4: Remove the hardcoded ROM define in `src/kernel.cpp`**

Delete these lines near the top:

```cpp
// ROM file to load from the SD card root.
// Circle's FatFS supports root directory only with 8.3 filenames.
// Copy your Genesis ROM to the SD card and rename it to this name.
#define ROM_TITLE "GAME.MD"
```

- [ ] **Step 5: Remove `Display.Initialize()` from `CKernel::Initialize()`**

Delete this block (the menu renders on the text console first; the game display is initialized after a ROM is chosen):

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

```

- [ ] **Step 6: Swap the mount + ROM-load flow in `CKernel::Run()`**

Replace this block:

```cpp
	// Mount the SD card filesystem.
	if (!m_SDCard.Mount ())
	{
		m_Logger.Write (FromKernel, LogPanic, "SD card mount failed");
		return ShutdownHalt;
	}

	// Load ROM from the SD card root.
	// Rename your ROM to ROM_TITLE on the SD card before booting.
	m_Logger.Write (FromKernel, LogNotice, "Loading ROM: %s", ROM_TITLE);
	if (!m_SDCard.ReadFile (ROM_TITLE, &m_pROMBuffer, &m_nROMSize))
	{
		m_Logger.Write (FromKernel, LogPanic,
			"ROM not found — copy your Genesis ROM to the SD card root as %s",
			ROM_TITLE);
		return ShutdownHalt;
	}
```

with:

```cpp
	// Mount the SD card filesystem.
	if (!m_Storage.Mount ())
	{
		m_Logger.Write (FromKernel, LogPanic, "SD card mount failed");
		return ShutdownHalt;
	}

	// Browse SD:/roms and let the user pick a ROM (renders on the console).
	char romPath[300];
	if (!m_RomMenu.Run (romPath, sizeof romPath))
	{
		m_Logger.Write (FromKernel, LogPanic, "No ROMs found in /roms");
		return ShutdownHalt;
	}

	// Read the selected ROM.
	m_Logger.Write (FromKernel, LogNotice, "Loading ROM: %s", romPath);
	if (!m_Storage.ReadFile (romPath, &m_pROMBuffer, &m_nROMSize))
	{
		m_Logger.Write (FromKernel, LogPanic, "Failed to read ROM: %s", romPath);
		return ShutdownHalt;
	}

	// Hand the screen over from the menu console to the game framebuffer.
	if (!m_Display.Initialize ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Display init failed");
		return ShutdownHalt;
	}
```

- [ ] **Step 7: Point `gameInfo.path` at the selected ROM in `src/kernel.cpp`**

Replace:

```cpp
	gameInfo.path = ROM_TITLE;
```

with:

```cpp
	gameInfo.path = romPath;
```

- [ ] **Step 8: Remove `sdcard.o` from `OBJS` in `Makefile`**

Delete this line from `OBJS`:

```make
       src/storage/sdcard.o \
```

- [ ] **Step 9: Drop the built-in FS libraries in `Makefile`**

In `LIBS`, delete these two lines:

```make
       $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
```

Also delete their two build-rule blocks:

```make
$(CIRCLEHOME)/lib/fs/fat/libfatfs.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

$(CIRCLEHOME)/lib/fs/libfs.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

```

- [ ] **Step 10: Delete the retired SDCard wrapper**

```bash
git rm src/storage/sdcard.h src/storage/sdcard.cpp
```

- [ ] **Step 11: Cross-build to verify it compiles and links**

Run: `make`
Expected: compiles `src/kernel.o`, links without `CFATFileSystem`/`SDCard`, ends with `WC  kernel7.img => <size>`, exit 0.

- [ ] **Step 12: Run the host suite (regression)**

Run: `make -C test run`
Expected: all five suites pass.

- [ ] **Step 13: Commit**

```bash
git add src/kernel.h src/kernel.cpp Makefile
git commit -m "ROM menu: boot into the ROM browser; retire CFATFileSystem/SDCard"
```

---

## Hardware verification (after Task 6)

Flash `kernel7.img`. On the SD card, create a `roms/` folder with several Genesis ROMs (long names OK) and at least one subfolder containing a ROM. With a USB gamepad connected at boot:

- [ ] The browser lists the ROMs (and `[subfolder]` entries), header shows `SD:/roms`.
- [ ] D-pad Up/Down moves the `>` cursor; a list longer than the screen scrolls.
- [ ] Start on a `[subfolder]` enters it and shows a `[..]` back entry; Start on `[..]` returns to the parent; the browser never goes above `/roms`.
- [ ] Start on a ROM loads and runs it (game framebuffer takes over).
- [ ] With an empty/absent `roms/`, the "No ROMs found" message appears instead of crashing.

---

## Notes for the implementer

- **`SimpleMsDelay`/`UpdatePlugAndPlay`** run at TASK_LEVEL in the menu loop — fine (same context as the game loop).
- **Edge detection** (`pressed = now & ~prev`) gives one move per press; holding a direction does not auto-repeat (deferred by design).
- **`GP_UP`/`GP_DOWN`/`GP_START`** come from `src/input/joypad_map.h`: directions are synthesized from the pad's hat/axes in `gamepad.cpp`; `GP_START` is the raw Start bit (`0x200`). The menu reuses the M7 gamepad as-is.
- **Volume string:** FatFS uses `"SD:"` (drive 0 per `FF_VOLUME_STRS`), which the add-on `diskio.cpp` binds to the `emmc1` device. FatFS reads the partition table itself, so no `-1` partition suffix is needed.
- **Display handoff:** the menu draws on `m_Screen`; `m_Display.Initialize()` is called only after a ROM is selected, so the game framebuffer replaces the console at that point.
```
