# Region + Auto-Launch Settings — Design

**Date:** 2026-06-20
**Status:** approved design, pending implementation plan
**Related:** FSD §4.9.2 (config table: `region`, `auto_launch_rom`), FSD §4.1 (auto-launch boot behavior), the settings subsystem and Settings screen added for video/audio settings.

## Summary

Wire two more FSD §4.9 settings into the existing settings subsystem:

- **`region`** — Genesis region / refresh rate (`auto` | `ntsc` | `pal`), fed to the
  core's `genesis_plus_gx_wide_region_detect` variable; a Settings-screen row,
  applied on next ROM load.
- **`auto_launch_rom`** — a ROM path that boots directly into a game, bypassing
  the ROM browser. Set/cleared via a Settings-screen "Auto-launch this game"
  toggle (no on-screen text entry), honored at boot.

Both reuse the `Settings` struct, `SettingsStore` (`SD:/settings.txt`), and the
in-emulation Settings screen.

## Region

### Model & mapping
- `enum class Region { Auto, NTSC, PAL };` added to `Settings`; field `region`,
  default `Auto`. File key `region=auto|ntsc|pal`.
- Pure helper `const char *region_core_value(Region r)` maps to the core's option
  strings: `Auto → "auto"`, `NTSC → "ntsc-u"`, `PAL → "pal"`. (The core also
  supports `"ntsc-j"`; the FSD exposes only auto/ntsc/pal, so `ntsc → ntsc-u`.)

### Core wiring
- `environment.{h,cpp}` gains `extern const char *g_region_value;` (default
  `"auto"`). The `RETRO_ENVIRONMENT_GET_VARIABLE` handler answers
  `genesis_plus_gx_wide_region_detect` with `g_region_value`, alongside the
  existing widescreen case. Changing it sets `g_variables_dirty` (the existing
  `GET_VARIABLE_UPDATE` flag) so the core re-reads.

### Apply timing
Applies on **next ROM load**. NTSC↔PAL changes the frame rate (≈59.92↔50 Hz),
and the kernel reads `retro_get_system_av_info` timing (which drives the pacing
loop) only at ROM load. A bare reset may switch the core's region but would not
re-sync our pacing fps; a reload does. The Settings-screen row is annotated
"applies on reload".

## Auto-Launch

### Model
- `Settings` gains `char auto_launch_rom[256]` (default empty `""`). File key
  `auto_launch_rom=SD:/roms/<path>` (empty when unset).

### Setting / clearing (no text entry)
- The Settings screen is told the current ROM path via
  `SettingsScreen::SetCurrentRom(const char *path)` (the kernel calls this on each
  ROM load).
- A Settings-screen row **"Auto-launch this game: < On | Off >"**:
  - **On** copies the current ROM path into `auto_launch_rom`.
  - **Off** clears `auto_launch_rom`.
  - The row displays **On** when `auto_launch_rom` equals the current ROM path,
    else **Off**. (Only one auto-launch ROM exists; toggling On for a different
    game overwrites it.)
- Persisted via `SettingsStore::Save` like every other setting.

### Boot flow
In the kernel's browse→play loop, a `firstBoot` flag gates auto-launch:
- On the **first** pass, if `auto_launch_rom[0] != '\0'` **and**
  `Storage::Exists(auto_launch_rom)`, load that ROM directly, skipping
  `RomMenu::Run()`.
- Otherwise show the ROM browser as today.
- `firstBoot` is cleared after the first pass, so after pause → "Return to ROM
  Browser" every later pass shows the browser — the browser is always reachable
  (the escape hatch).

## Settings Screen (grows 4 → 6 rows)

```
Settings

  Video Scale:            < Integer | Stretch >
  Widescreen:             < Off | On >   (applies on reset)
  Volume:                 < 100 >
  Mute:                   < Off >
  Region:                 < Auto | NTSC | PAL >   (applies on reload)
  Auto-launch this game:  < Off | On >

  B: back
```

`Apply()` additionally sets `g_region_value = region_core_value(region)` and
`g_variables_dirty = true`. The auto-launch row edits `auto_launch_rom` against
the current ROM path; it does not need `Apply()` (nothing live to push) but is
saved like the others.

## Error Handling

- `region`: invalid/missing value → `Auto`.
- `auto_launch_rom`: a value longer than the buffer is truncated (and then simply
  won't match `Exists`, falling back to the browser).
- Boot: a configured auto-launch path that is missing/unreadable falls back to
  `RomMenu` (logged via serial; never bricks boot).
- The "Auto-launch this game" row only appears meaningful during play (the
  Settings screen is reached from the pause menu, so a current ROM always
  exists); `SetCurrentRom` is always called before the screen can be opened.

## Testing

**Host unit tests** (`test/test_settings.cpp` extended):
- `region` parse: `auto`/`ntsc`/`pal` → the right enum; invalid/missing → `Auto`;
  round-trip via `serialize_settings`.
- `auto_launch_rom` parse + round-trip, including empty (unset) and a path
  containing spaces (e.g. `SD:/roms/Streets of Rage.md`).
- `region_core_value`: `Auto→"auto"`, `NTSC→"ntsc-u"`, `PAL→"pal"`.

**Hardware verification:**
- Set Region = PAL, return to browser, reload a game → runs at 50 Hz (visibly /
  audibly slower than NTSC); set NTSC → ≈60 Hz.
- Set "Auto-launch this game" On, power-cycle → boots straight into that ROM
  (no browser). Pause → "Return to ROM Browser" reaches the browser.
- Clear it, power-cycle → the ROM browser is shown at boot.
- Inspect `SD:/settings.txt` → `region` and `auto_launch_rom` persisted.

## Out of Scope

- On-screen text entry / a file picker for arbitrary auto-launch paths (the
  toggle covers the in-menu workflow).
- `ntsc-j` region exposure (core supports it; FSD does not list it).
- The remaining FSD §4.9 key `menu_hotkey` (separate future work).
