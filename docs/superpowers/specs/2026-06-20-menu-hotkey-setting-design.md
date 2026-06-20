# Menu Hotkey Setting — Design

**Date:** 2026-06-20
**Status:** approved design, pending implementation plan
**Related:** FSD §4.9.2 (config key `menu_hotkey`, default `start+select`); the settings subsystem and Settings screen (video/audio/region settings).

## Summary

Make the in-emulation menu hotkey (currently the hardcoded `Start+Select`)
user-configurable via a preset list, completing the FSD §4.9 config table. The
hotkey is chosen from a small fixed set of button combos on the Settings screen,
stored in `SD:/settings.txt`, and applied live.

## Background

Today the kernel hardcodes `const unsigned HOTKEY = GP_START | GP_SELECT;` and the
play loop opens the pause menu when that combo is held (checked against the
combined `MenuButtons()` of both pads). This design replaces the constant with a
setting.

## Model

- `enum class MenuHotkey { StartSelect, StartA, StartB, LR };` added to
  `Settings`; field `menu_hotkey`, default `StartSelect`.
- File key `menu_hotkey` with values `start+select` | `start+a` | `start+b` | `l+r`.
- Pure helpers (in `src/settings/settings.cpp`, which includes the pure
  `src/input/joypad_map.h` for the `GP_*` bit defines):
  - `unsigned hotkey_mask(MenuHotkey h)` → the button bitmask:
    - `StartSelect → GP_START | GP_SELECT`
    - `StartA → GP_START | GP_A`
    - `StartB → GP_START | GP_B`
    - `LR → GP_LB | GP_RB`
  - `const char *menu_hotkey_file_value(MenuHotkey h)` → the keyword
    (`"start+select"` / `"start+a"` / `"start+b"` / `"l+r"`), for serialize.

The four presets are combos unlikely to be pressed together during normal play.

## Wiring

### Kernel (`src/kernel.cpp`)
Remove the hardcoded `const unsigned HOTKEY = GP_START | GP_SELECT;`. In the play
loop, compute the mask per frame from the live settings:

```cpp
unsigned hotkey = hotkey_mask (m_Settings.menu_hotkey);
```

The existing detection (`(pressed & hotkey) && (now & hotkey) == hotkey`)
is unchanged. Because the `SettingsScreen` edits the same `m_Settings`, a change
takes effect on the next frame after the menu closes — **live, no reset**. No
`Apply()` hook is needed for this setting (nothing in the core/audio/video to
push; the kernel reads it directly).

### Settings screen (`src/menu/settings_screen.{h,cpp}`)
Add a 7th row: `Menu Hotkey: < Start+Select | Start+A | Start+B | L+R >`,
cycling the four presets (left/right), with friendly display labels. Editing sets
`m_pSettings->menu_hotkey` and persists via `SettingsStore::Save`, like the other
rows. (The hotkey value is read by the kernel, so the row does not call
`Apply()`.)

## Error Handling

- Parse: an unknown/missing `menu_hotkey` value → default `StartSelect`.
- The mask is always a valid two-button combo; no empty/invalid mask is possible.

## Testing

**Host unit tests** (`test/test_settings.cpp` extended; include `joypad_map.h`
for the `GP_*` defines):
- Parse each keyword → the right enum; invalid/missing → `StartSelect`.
- Round-trip via `serialize_settings`.
- `hotkey_mask` for all four presets equals the expected `GP_*` combination.
- `menu_hotkey_file_value` returns the right keyword for each.

**Hardware verification:**
- Set Menu Hotkey = L+R, resume → L+R opens the pause menu; Start+Select no longer
  does.
- Power-cycle → the choice persists in `SD:/settings.txt`.

## Out of Scope

- Press-to-bind capture of arbitrary combos (the preset list covers the in-menu
  workflow without text entry).
- Single-button or three-button hotkeys.

This is the final FSD §4.9 config-table key; with it the settings table
(`auto_launch_rom`, `region`, `video_scale`, `widescreen`, `audio_*` partial,
`controller_*` deferred, `menu_hotkey`) is complete except for the explicitly
deferred controller-mapping keys.
