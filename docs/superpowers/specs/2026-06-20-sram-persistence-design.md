# SRAM Persistence — Design

**Date:** 2026-06-20
**Status:** Approved (pending implementation plan)
**Milestone:** Phase 2

## Summary

Persist battery-backed cartridge SRAM (in-game saves for RPGs etc.) to the SD
card so progress survives power cycles. On ROM load, SRAM is read from
`SD:/saves/<rom>.srm` into the core; during play it is written back periodically
(only when it changed) and again when returning to the ROM browser. Fully
automatic — no UI.

## Goals

- Load SRAM from `SD:/saves/<rom>.srm` at ROM load time, if the game has SRAM and
  the file exists.
- Write SRAM back periodically during emulation (only when it changed) and on
  return to the ROM browser.
- Be a no-op for games without SRAM (no file, no overhead).
- Survive a power-cut with at most ~10 s of lost progress (there is no safe
  shutdown on bare metal).

## Non-Goals (deferred)

- A UI / status indicator for SRAM (it is fully automatic).
- RTC-backed cartridges (`RETRO_MEMORY_RTC`), or other memory regions.
- Per-slot SRAM backups (single `.srm` per game).
- A manual "flush SRAM now" menu action.

## Decisions (from brainstorming)

1. **Timing: periodic (dirty-checked) + on ROM change.** Auto-save every ~10 s
   but only when the SRAM actually changed since the last write (minimises SD
   writes/wear and limits the gameplay hitch to just after an in-game save), plus
   a save when returning to the browser. Periodic saving matters because the Pi
   has no safe shutdown — power is simply cut.
2. **Approach A:** a dedicated `Sram` manager (mirrors `SaveState`), with a pure
   host-tested `sram_path` helper.

## Architecture

### File structure

| Unit | Responsibility | Dependencies | Tested |
|---|---|---|---|
| `src/menu/save_path.{h,cpp}` (modify) | Add pure `sram_path`. | none | host |
| `src/menu/sram.{h,cpp}` (new) | `Sram` manager. | `Storage`, libretro | on-hardware |
| `src/kernel.{h,cpp}` (modify) | Wire `Sram` into load / play loop / unload. | — | — |

`Sram` lives in `src/menu/` next to `SaveState`; both are save-persistence
managers reusing `save_path`, keeping the existing menu→storage include direction.

### Path helper (`src/menu/save_path.{h,cpp}`) — pure

Add:

```cpp
// out = "SD:/saves/<filename>.srm", where <filename> is romPath with its
// directory stripped (extension kept). Bounded by out_size.
void sram_path(const char *romPath, char *out, unsigned out_size);
```

Example: `sram_path("SD:/roms/Genesis/Phantasy Star.md", …)` →
`SD:/saves/Phantasy Star.md.srm`. Shares the basename logic with `state_path`.

### Sram manager (`src/menu/sram.{h,cpp}`)

```cpp
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
    u64      m_LastCheck;   // CTimer ticks of the last Tick action
};
```

- `INTERVAL` = 10 s (in CTimer ticks/µs).
- `Present(&data,&size)`: `size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)`,
  `data = (u8 *) retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)`; returns
  `size > 0 && data != 0`.
- `SetGame(romPath)`: copy the path; `delete[] m_pSnapshot`; if `Present`, allocate
  `m_pSnapshot[size]`, seed it from the core's current SRAM, set `m_SnapSize = size`;
  else `m_pSnapshot = 0`, `m_SnapSize = 0`. Reset `m_LastCheck` to now.
- `Load()`: if not `Present`, return. Build `sram_path`; `Storage::ReadFile` (returns
  false if the file is absent — just return). **Only if the file size equals the
  SRAM size**, `memcpy` the file into the core SRAM and into the snapshot; free the
  read buffer. (A size mismatch is logged and skipped so a stale/foreign file can't
  corrupt the core.)
- `Save()`: if not `Present`, return. `m_pStorage->MakeDir("SD:/saves")`;
  `m_pStorage->WriteFile(sram_path, data, size)`; on success `memcpy` SRAM into the
  snapshot.
- `Tick()`: if not `Present`, return. `now = CTimer::GetClockTicks64()`; if
  `now - m_LastCheck < INTERVAL`, return; `m_LastCheck = now`; if `memcmp(data,
  m_pSnapshot, size) != 0`, write (as in `Save`) and refresh the snapshot.
- Destructor: `delete[] m_pSnapshot`.

### Kernel changes (`src/kernel.{h,cpp}`)

- New member `Sram m_Sram;` constructed with `&m_Storage` (declared after
  `m_SaveState`).
- After `retro_load_game` (next to `m_SaveState.SetGame(romPath)`):
  `m_Sram.SetGame(romPath); m_Sram.Load();`.
- In the play loop, after `retro_run()`: `m_Sram.Tick();`.
- On return-to-browser, immediately before `retro_unload_game()`: `m_Sram.Save();`.

## Data flow

```
load game:
    retro_load_game(); m_SaveState.SetGame(romPath)
    m_Sram.SetGame(romPath); m_Sram.Load()
        Load: if Present && Exists("SD:/saves/<rom>.srm"):
            ReadFile; if filesize == SRAM size: file -> core SRAM; snapshot <- SRAM
play loop (per frame):
    retro_run(); m_Sram.Tick(); pace ...
        Tick: if Present && now-lastCheck >= 10s:
            lastCheck = now
            if memcmp(SRAM, snapshot): WriteFile(SRAM); snapshot <- SRAM
return to ROM browser:
    m_Sram.Save(); retro_unload_game(); delete[] ROM buffer
        Save: if Present: MakeDir; WriteFile(SRAM); snapshot <- SRAM
```

An in-game save reaches the SD card within ~10 s, and immediately on
return-to-browser. While the pause menu is open the play loop isn't running, so
`Tick` doesn't fire (SRAM can't change then anyway). A `retro_reset` or a
save-state Load that alters SRAM is caught by the next `Tick`.

## Error handling

| Condition | Behavior |
|---|---|
| Game has no SRAM (`size == 0`) | `Present` false → Load/Save/Tick no-op; no `.srm`, no hitch. |
| `.srm` missing at load | Nothing loaded; game starts with the core's fresh SRAM. |
| `.srm` size ≠ SRAM size | Skip the load (no corruption); log. Next save rewrites it correctly. |
| `WriteFile` fails | Best-effort: log and continue; next `Tick`/`Save` retries. |
| `saves/` absent | `Save`/`Tick` `MakeDir` first (`FR_EXIST` = ok). |

## Testing

**Host (`test/`):**
- `sram_path`: strips the directory, keeps the filename + extension, appends
  `.srm`; root ROM and subfolder ROM both yield `SD:/saves/<file>.srm`; respects
  `out_size`. Existing suites still pass.

**Hardware:**
- A game with SRAM (an RPG): make an in-game save, wait ~10 s, power-cycle, reload
  the ROM → the in-game save is present.
- In-game save, return to the ROM browser, relaunch → save persists.
- A game without SRAM: no `.srm` is created; no periodic hitch.
- A truncated/foreign `.srm`: ignored at load (game still boots), then overwritten
  by the next save.

## Acceptance criteria

- ROMs with battery saves load their SRAM from `SD:/saves/<rom>.srm` and write it
  back periodically (when changed) and on return-to-browser, so in-game progress
  survives power cycles.
- ROMs without SRAM create no file and incur no periodic write.
- A mismatched/missing `.srm` never corrupts a running game.
- Host test for `sram_path` passes; existing host suites still pass.
