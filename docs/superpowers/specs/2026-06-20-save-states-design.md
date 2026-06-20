# Save States — Design

**Date:** 2026-06-20
**Status:** Approved (pending implementation plan)
**Milestone:** Phase 2

## Summary

Add save/load of the full emulator state to four SD-card slots per game, driven
from the in-emulation menu. Selecting Save State or Load State opens a slot
picker (slots 1–4, Empty/Used); save serializes the core to
`SD:/saves/<rom>.state<N>`, load restores it. This lights up the Save State /
Load State entries that the in-emulation menu currently shows greyed.

## Goals

- Save the complete emulator state (`retro_serialize`) to a chosen slot file.
- Load a previously saved state (`retro_unserialize`) from an occupied slot,
  restoring the game exactly.
- Four slots per game; the picker shows each slot as Empty or Used.
- Save/load reachable from the in-emulation menu (Save State / Load State).
- Saves persist on the SD card across power cycles.

## Non-Goals (deferred)

- Date/time on saves (no real-time clock on the bare-metal Pi; slots show only
  Empty/Used). Adding an RTC is out of scope.
- Save-state screenshots / thumbnails.
- Auto-save / save-on-exit, rewind, or in-game quick-save hotkeys.
- SRAM (battery) persistence — a separate feature.
- Confirmation prompt before overwriting a Used slot (overwrites silently; the
  slot is shown as Used).

## Decisions (from brainstorming)

1. **Four slots with a picker.** Save State / Load State open a slot submenu
   (1–4). Save allows any slot (overwrites); Load allows only occupied slots.
2. **Empty/Used status only** — no timestamps (no RTC).
3. **Approach A:** a `SaveState` manager (serialize + storage) plus a slot-picker
   sub-flow inside `PauseMenu`; the kernel only sets the current game per load.
   Path logic is a pure, host-tested helper.

## Architecture

### File structure

| Unit | Responsibility | Dependencies | Tested |
|---|---|---|---|
| `src/storage/storage.{h,cpp}` (modify) | Add `WriteFile`, `Exists`, `MakeDir`. | ChaN FatFS | on-hardware |
| `src/menu/save_path.{h,cpp}` (new) | Pure `state_path`. | none | host |
| `src/menu/save_state.{h,cpp}` (new) | `SaveState` manager. | `Storage`, libretro | on-hardware |
| `src/menu/pause_menu.{h,cpp}` (modify) | Enable Save/Load; slot picker; messages. | `SaveState`, `TextCanvas` | on-hardware |
| `src/kernel.{h,cpp}` (modify) | `SaveState` member; `SetGame` per load. | — | — |

### Storage additions (`src/storage/storage.{h,cpp}`)

```cpp
bool WriteFile(const char *path, const u8 *data, size_t size);
bool Exists(const char *path);
bool MakeDir(const char *path);
```

- `WriteFile`: `f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS)`, `f_write` the whole
  buffer, `f_close`; false on any FatFS error or short write.
- `Exists`: `FILINFO fno; return f_stat(path, &fno) == FR_OK;`.
- `MakeDir`: `FRESULT r = f_mkdir(path); return r == FR_OK || r == FR_EXIST;`.

### Path helper (`src/menu/save_path.{h,cpp}`) — pure

```cpp
// Build "SD:/saves/<filename>.state<slot>" from a ROM path, where <filename> is
// romPath with its directory stripped (extension kept). slot is 1..4. Bounded.
void state_path(const char *romPath, int slot, char *out, unsigned out_size);
```

Example: `state_path("SD:/roms/Genesis/Sonic.md", 2, …)` →
`SD:/saves/Sonic.md.state2`.

### SaveState manager (`src/menu/save_state.{h,cpp}`)

```cpp
class SaveState
{
public:
    SaveState(Storage *pStorage);
    void SetGame(const char *romPath);   // copies romPath (current game)

    bool Occupied(int slot);             // a save file exists for this slot
    bool Save(int slot);                 // serialize core -> slot file
    bool Load(int slot);                 // slot file -> unserialize core

private:
    Storage *m_pStorage;
    char     m_romPath[300];
};
```

- `Save(slot)`: `m_pStorage->MakeDir("SD:/saves")`; `size = retro_serialize_size()`
  (return false if 0); `u8 *buf = new u8[size]`; `if (!retro_serialize(buf, size))
  { delete[]; return false; }`; `bool ok = m_pStorage->WriteFile(path, buf, size)`;
  `delete[] buf`; return `ok`. (`path` from `state_path(m_romPath, slot, …)`.)
- `Load(slot)`: `u8 *buf; size_t size; if (!m_pStorage->ReadFile(path, &buf,
  &size)) return false; bool ok = retro_unserialize(buf, size); delete[] buf;
  return ok;`.
- `Occupied(slot)`: `m_pStorage->Exists(path)`.

### PauseMenu changes (`src/menu/pause_menu.{h,cpp}`)

- Constructor gains `SaveState *pSaveState`.
- Enabled entries become: Resume, Save State, Load State, Reset Game, Return to
  ROM Browser (Settings stays disabled). `ENABLED = {true,true,true,true,false,true}`.
- Confirm handling by index (replacing the simple action table):
  - 0 Resume → return `MenuAction::Resume`.
  - 1 Save State → `int slot = PickSlot(false)`; if `slot != 0`:
    `bool ok = m_pSaveState->Save(slot)`; `Message(ok ? "Saved to slot N" :
    "Save failed")`; redraw the pause menu (stay open).
  - 2 Load State → `int slot = PickSlot(true)`; if `slot != 0`:
    `if (m_pSaveState->Load(slot)) return MenuAction::Resume;` else
    `Message("Load failed")`; redraw.
  - 3 Reset Game → return `MenuAction::Reset`.
  - 5 Return to ROM Browser → return `MenuAction::ReturnToBrowser`.
- `int PickSlot(bool forLoad)` (private): draws a box listing "Slot 1: Empty/Used"
  … "Slot 4: …" (status via `m_pSaveState->Occupied`). For load, the selectable
  mask is the occupied slots (navigate with `menu_next_enabled`); for save, all
  four are selectable. `Start` selects (returns 1–4); `B` cancels (returns 0). If
  `forLoad` and no slot is occupied, the initial selection has nothing enabled —
  the user backs out with `B`.
- `void Message(const char *text)` (private): draw the text in the box and hold
  ~1.2 s (`CTimer::SimpleMsDelay`), then return.

### Kernel changes (`src/kernel.{h,cpp}`)

- New member `SaveState m_SaveState;` constructed with `&m_Storage` (declared after
  `m_Storage`, before `m_PauseMenu`).
- `m_PauseMenu` constructed with the extra `&m_SaveState` argument.
- After each successful `retro_load_game`, call `m_SaveState.SetGame(romPath)`
  before entering the play loop.
- The play loop and the Resume/Reset/Return dispatch are otherwise unchanged.

## Data flow

```
gameplay --Start+Select--> PauseMenu
  Save State -> PickSlot(false) [all selectable]
      slot N -> SaveState.Save(N):
          MakeDir("SD:/saves"); size=retro_serialize_size();
          retro_serialize(buf,size); WriteFile("SD:/saves/<rom>.stateN")
      Message("Saved to slot N" | "Save failed"); redraw menu
  Load State -> PickSlot(true) [Used selectable only]
      slot N -> SaveState.Load(N):
          ReadFile("SD:/saves/<rom>.stateN") -> buf,size; retro_unserialize
      success -> return Resume (kernel ForceRepaint + resume)
      failure -> Message("Load failed"); redraw menu
```

`SetGame(romPath)` is set per game, so paths always target the running ROM. The
kernel already calls `Display.ForceRepaint()` on Resume, so a loaded frame
repaints cleanly.

## Error handling

| Condition | Behavior |
|---|---|
| `retro_serialize_size() == 0` | "Save states unavailable" message; stay in menu. |
| `retro_serialize` or `WriteFile` fails | "Save failed" message; stay in menu. |
| `MakeDir("SD:/saves")` when it already exists | Success (`FR_EXIST`). |
| Load an empty slot | Not selectable in the picker. |
| `ReadFile` / `retro_unserialize` fails | "Load failed"; game continues unchanged. |
| Overwrite a Used slot | Silent overwrite (slot shown as Used). |

## Testing

**Host (`test/`):**
- `state_path`: strips the directory and keeps the extension; formats the slot
  digit (1–4); root ROM and subfolder ROM both yield `SD:/saves/<file>.stateN`;
  respects `out_size`.

**Hardware:**
- Save to slot 2 mid-game; keep playing; Load slot 2 → state restored exactly.
- The picker shows Empty vs Used correctly; an Empty slot can't be chosen for Load.
- Overwrite a Used slot, then Load it → restores the newer state.
- Power-cycle, boot the same ROM, Load the slot → restores (saves persist).
- A deliberately truncated/foreign `.stateN` file → "Load failed", no crash.

## Acceptance criteria

- Save State writes `SD:/saves/<rom>.state<N>` for the chosen slot; Load State
  restores the emulator from an occupied slot exactly.
- The slot picker shows all four slots as Empty/Used; Load offers only Used slots;
  Save offers all four.
- Saves persist across power cycles; a failed load leaves the game running.
- Host test for `state_path` passes; existing host suites still pass.
