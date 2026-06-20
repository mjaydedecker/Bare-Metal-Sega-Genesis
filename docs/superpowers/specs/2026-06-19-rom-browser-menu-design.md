# ROM Browser Menu — Design

**Date:** 2026-06-19
**Status:** Approved (pending implementation plan)
**Milestone:** Phase 2, first feature

## Summary

On boot, present a scrollable on-screen menu of the Genesis ROMs found in the
SD card's `/roms` folder. The user navigates with a USB gamepad and presses a
button to load and launch the selected ROM. This replaces the current hardcoded
`GAME.MD` load.

## Goals

- Scan `/roms` on the SD card for Genesis ROM files (`.md`, `.bin`, `.gen`).
- Render a scrollable, single-selection list of filenames on the HDMI display.
- Navigate with the gamepad (D-pad Up/Down) and launch with a button (Start).
- Show a clear message when no ROMs are present.
- Long filenames and a `/roms` subdirectory (not just 8.3 names in the root).

## Non-Goals (deferred)

- `config.txt` auto-launch (skip the menu for a configured default ROM).
- Returning to the browser from a running game (needs core reset / in-emulation
  menu — a separate feature).
- Sorting options, search/filter, folders/subdirectory navigation below `/roms`.
- USB keyboard navigation (gamepad only for now).
- Auto-repeat when a direction is held (single step per press).
- Box art / thumbnails / metadata.

## Decisions (from brainstorming)

1. **ROM location & filenames:** `/roms` folder with long filenames. This
   requires switching from Circle's built-in `CFATFileSystem` (root-only, 8.3)
   to the add-on ChaN FatFS (`addon/fatfs`: `f_opendir`/`f_readdir`, LFN +
   subdirectories). LFN is already enabled in `ffconf.h` (`FF_USE_LFN=3`,
   `FF_MAX_LFN=255`); `FF_FS_RPATH=2`. Circle ships the EMMC glue (`diskio.cpp`).
2. **Input:** USB gamepad only — reuse the M7 `Gamepad`. D-pad Up/Down moves the
   selection; Start launches.
3. **Rendering:** text console via `CScreenDevice` — print the visible
   filenames, mark the selection with a `>` cursor, redraw on change.
4. **Scope:** minimal — browse, pick, launch (runs until reboot); "No ROMs
   found" message when empty.

## Architecture

Approach A: layered, host-testable modules. The fiddly logic (extension
matching, scroll/selection math) lives in pure, Circle-free functions with host
tests — matching the existing pattern (`blit_rgb565`, `joypad_state`). The
Circle-bound shell stays thin.

### File structure

| Unit | Responsibility | Dependencies | Tested |
|---|---|---|---|
| `src/storage/storage.{h,cpp}` | ChaN-FatFS storage layer (replaces `sdcard.*`). | ChaN `ff.h`, EMMC | on-hardware |
| `src/menu/rom_filter.{h,cpp}` | Pure `is_rom_filename`. | none | host |
| `src/menu/menu_state.{h,cpp}` | Pure scroll/selection math. | none | host |
| `src/menu/rom_menu.{h,cpp}` | `RomMenu` — render + input + delegate. | `CScreenDevice`, `Gamepad`, `Storage`, `CUSBHCIDevice` | on-hardware |
| `src/kernel.{h,cpp}` | Wire mount → menu → load → emulate. | — | — |

### Storage (`src/storage/storage.{h,cpp}`)

Replaces M3's `CFATFileSystem`-based `SDCard`. Holds a ChaN `FATFS` object.

```cpp
struct RomEntry { char name[256]; };   // filename only; path is "/roms/" + name

class Storage
{
public:
    Storage(void);
    bool Mount(void);                                   // f_mount over the EMMC volume
    int  ListRoms(const char *dir, RomEntry *out, int max);  // count, or -1 on error
    bool ReadFile(const char *path, u8 **ppBuffer, unsigned *pSize);
private:
    FATFS m_FS;
};
```

- `Mount()`: `f_mount(&m_FS, "SD:", 1)`. Circle's `addon/fatfs` `diskio.cpp` maps
  the `SD` volume (drive 0, per `FF_VOLUME_STRS`) to the `emmc1` device the EMMC
  init registers. All FatFS paths are volume-qualified, e.g. `SD:/roms`. (The
  user-facing folder is still "/roms" on the card.)
- `ListRoms()`: `f_opendir(dir)` then loop `f_readdir`; skip directories; keep
  entries where `is_rom_filename(fno.fname)` is true; copy the (long) name into
  `out[]` up to `max`. Returns the count, or `-1` if the directory can't be
  opened (e.g. `/roms` missing).
- `ReadFile()`: `f_open(path, FA_READ)`, allocate `f_size(&file)` bytes, `f_read`
  the whole file, return the buffer + size. Buffer lives for the session (no
  free; no return-to-menu).

### ROM filter (`src/menu/rom_filter.{h,cpp}`) — pure

```cpp
bool is_rom_filename(const char *name);
```

True if `name` ends (case-insensitively) in `.md`, `.bin`, or `.gen`. A name
that is only an extension (e.g. `.md`) or has no extension is false.

### Menu state (`src/menu/menu_state.{h,cpp}`) — pure

```cpp
struct MenuState
{
    int count;         // number of entries
    int selected;      // 0..count-1
    int top;           // index of the first visible row
    int visible_rows;  // rows that fit on screen
};

void menu_move(MenuState *s, int delta);  // move selection by delta, clamp, rescroll
```

`menu_move` clamps `selected` to `[0, count-1]` and adjusts `top` so `selected`
stays within `[top, top + visible_rows - 1]` (scroll down when moving past the
bottom, up when moving above the top). With `count <= visible_rows`, `top`
stays 0. Safe for `count == 0` and `count == 1`.

### RomMenu (`src/menu/rom_menu.{h,cpp}`) — Circle shell

```cpp
class RomMenu
{
public:
    RomMenu(CScreenDevice *pScreen, Gamepad *pGamepad,
            Storage *pStorage, CUSBHCIDevice *pUSBHCI);
    // Returns true and writes "SD:/roms/<name>" into outPath on launch.
    // Returns false only on the no-ROMs terminal state (caller halts).
    bool Run(char *outPath, unsigned outSize);
private:
    void Render(const MenuState &s, const RomEntry *entries);
    /* members: the four pointers above */
};
```

`Run()`:
1. `Storage.ListRoms("SD:/roms", entries, MAX)`. If `<= 0`, render the "No ROMs
   found. Place .md/.bin/.gen files in /roms" message and return `false`.
2. Build `MenuState{count, 0, 0, visible_rows}`; `Render` once.
3. Loop (~16 ms/iteration via a short timer delay):
   - `m_USBHCI->UpdatePlugAndPlay()` (gamepad enumerates here, as in the game
     loop).
   - `m_Gamepad->Poll()`; read `Buttons()`.
   - Edge-detect against `prevButtons`: a "press" is a bit set now and clear
     last iteration.
     - Up press → `menu_move(&s, -1)` and re-render if changed.
     - Down press → `menu_move(&s, +1)` and re-render if changed.
     - Start press → write `"SD:/roms/" + entries[s.selected].name` to `outPath`,
       return `true`.
   - Save `prevButtons`.

`Render()` clears the screen and prints a header plus the visible slice
(`entries[top .. top+visible_rows-1]`), prefixing the selected row with `> ` and
others with `  `. Uses `CScreenDevice` (no game `Display` yet).

Direction/confirm bits come from the M7 `GP_*` constants (`GP_UP`, `GP_DOWN`,
`GP_START`).

### Kernel integration (`src/kernel.{h,cpp}`)

- `kernel.h`: drop `CFATFileSystem m_FileSystem` and `SDCard m_SDCard`; add
  `Storage m_Storage;` and `RomMenu m_RomMenu;` (constructed with `&m_Screen`,
  `&m_Gamepad`, `&m_Storage`, `&m_USBHCI`).
- `Initialize()`: after USB/EMMC, call `m_Storage.Mount()` (halt on failure).
  **Remove** `m_Display.Initialize()` from here.
- `Run()`:
  1. `char path[300]; if (!m_RomMenu.Run(path, sizeof path)) { halt; }`
  2. `m_Storage.ReadFile(path, &m_pROMBuffer, &m_nROMSize)` (message + halt on
     failure).
  3. `m_Display.Initialize()` — game framebuffer takes over the screen.
  4. Existing core load + callback wiring + emulation loop, using `path` as the
     ROM path and `m_pROMBuffer`/`m_nROMSize`.

## Data flow

```
boot
  Initialize: screen, serial, logger, interrupt, timer, USBHCI(PnP), EMMC
              Storage.Mount()            [game Display NOT yet initialized]
  Run:
    RomMenu.Run(path):
        ListRoms("SD:/roms") --> entries
        if none: show message; return false  --> kernel halts
        loop: UpdatePlugAndPlay; Poll; edge-detect; menu_move/Render; Start->return path
    Storage.ReadFile(path) --> buffer
    Display.Initialize()                 [game framebuffer takes over]
    load core + wire callbacks + emulation loop
```

## Build changes

- Add `addon/fatfs` to the build: compile its `ff.c`, `diskio.cpp`,
  `ffsystem.cpp`, `ffunicode.c` (the add-on's own `libfatfs.a`) and link it in
  place of the built-in `lib/fs/fat` / `lib/fs`.
- Add `src/storage/storage.o`, `src/menu/rom_filter.o`, `src/menu/menu_state.o`,
  `src/menu/rom_menu.o` to `OBJS`; retire `src/storage/sdcard.o`.
- `test/Makefile`: add `test_rom_filter` and `test_menu_state` host test
  targets.

## Error handling

| Condition | Behavior |
|---|---|
| SD mount fails | Log error; halt (existing M3 behavior). |
| `/roms` missing or no ROMs | Render "No ROMs found. Place .md/.bin/.gen files in /roms"; halt. |
| `ReadFile` fails | Render "Failed to read ROM"; halt. |
| No gamepad connected | List sits idle; a pad that enumerates via PnP becomes usable (no error path). |

## Testing

**Host (`test/`):**
- `is_rom_filename`: `Sonic.md`, `GAME.BIN`, `x.gen`, mixed case → true;
  `readme.txt`, `noext`, bare `.md` → false.
- `menu_state`: clamp at `0` and `count-1`; scroll `top` down when moving past
  the visible bottom and up when moving above the top; `count <= visible_rows`
  keeps `top == 0`; single-entry and empty lists are safe.

**Hardware:**
- `/roms` with several ROMs including long names and more than one screenful:
  navigate, scroll, select, launch.
- Empty/absent `/roms`: the "No ROMs found" message appears.

## Acceptance criteria

- Booting with ROMs in `/roms` shows a navigable list; pressing Start on a
  selection loads and runs that game.
- A list longer than the screen scrolls as the selection moves.
- Long filenames display correctly.
- An empty/absent `/roms` shows the "No ROMs found" message instead of crashing.
- Host tests for `is_rom_filename` and `menu_state` pass.
