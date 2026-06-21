# Remappable Hotkey Bindings + Remap UI — Design (Spec B2)

**Status:** approved design — ready for implementation plan.
**Date:** 2026-06-21
**Final piece of deferred video enhancement #8** — makes the Spec B1 in-game
hotkeys user-configurable. Builds on
`docs/superpowers/specs/2026-06-21-ingame-hotkeys-toasts-design.md`.

After this, deferred video enhancements #1–#8 are all implemented.

## Problem

Spec B1 shipped in-game hotkeys with a **fixed** Select+button mapping. Users
can't change which buttons trigger quick-save/load, volume, HUD, or mute.

## Goal

Make each in-game action's combo user-configurable via a remap screen, persisted
to `settings.txt`, while preserving B1's input-suppression behavior. Each action
binds a **(hold, trigger)** pair; the hold is restricted to a non-gameplay "safe"
set so suppression stays viable.

Non-goals: d-pad directions as bindable buttons (kept out so we reuse `PadButton`/
`pad_bit` unchanged); arbitrary live-capture UX (we cycle, like `ControlsScreen`);
preventing conflicts (we allow + warn + resolve by priority); per-player hotkey
sets (hotkeys are player-1 only, as in B1).

## Binding model

- A binding is `struct HotkeyBinding { PadButton hold; PadButton trigger; }`.
- **hold** must come from the safe set **{Select, Start, L, R}** (non-movement
  buttons, so masking them while held doesn't break play).
- **trigger** is any `PadButton` except the action's own hold (no `hold ==
  trigger`). No d-pad (not in `PadButton`).
- Six actions in a fixed array; **array order defines decode priority**:

```cpp
// settings.h (settings data; PadButton already lives here)
struct HotkeyBinding { PadButton hold; PadButton trigger; };
enum { HK_QUICKSAVE, HK_QUICKLOAD, HK_VOLUP, HK_VOLDOWN, HK_TOGGLEHUD, HK_MUTE,
       HK_COUNT };
struct HotkeyBindings { HotkeyBinding b[HK_COUNT]; };
```

The `HK_*` index maps 1:1 to the existing `InGameAction` values.

## Generalized decoder (pure)

`decode_hotkey` is generalized to consult the bindings (replacing B1's hardcoded
mapping). B1's 2-arg overload is removed; its test and the kernel call move to the
3-arg form.

```cpp
// src/input/hotkey.h
//   includes joypad_map.h (pad_bit) and settings.h (HotkeyBindings, PadButton)
InGameAction decode_hotkey(unsigned held, unsigned pressed,
                           const HotkeyBindings &hk);
// for i in 0..HK_COUNT in order:
//   if (held & pad_bit(hk.b[i].hold)) && (pressed & pad_bit(hk.b[i].trigger))
//       return action_for_index(i);
// return InGameAction::None;
```

Stays pure/host-testable: `pad_bit` is in `joypad_map.cpp` (already host-linked by
`test_joypad`/`test_hotkey`); `PadButton`/`HotkeyBindings` come from the pure
`settings.h`.

## Settings serialization + defaults

Six new `settings.txt` keys, value `hold+trigger` using existing button tokens:

```
hotkey_quicksave=select+x
hotkey_quickload=select+y
hotkey_volup=select+l
hotkey_voldown=select+r
hotkey_togglehud=select+a
hotkey_mute=select+b
```

- **Serialize:** reuse `pad_button_token()` for each side, joined with `+`.
- **Parse:** add a small pure helper `bool pad_button_from_token(const char *tok,
  PadButton *out)` (token → `PadButton`), used to parse each side of `hold+trigger`.
- **Validation → default:** a malformed key, an out-of-set hold (not in {Select,
  Start, L, R}), or `hold == trigger` falls back to that action's default (mirrors
  the existing "invalid value → default" convention; keeps the file robust).
- **Defaults** (in the `Settings` constructor): hold = Select for all six;
  triggers `X/Y/A/B` for save/load/HUD/mute, `L/R` for volume up/down. (B1 used
  d-pad Up/Down for volume; B2 moves those to the shoulders because the d-pad is
  not a `PadButton`. This is the one default-behavior change vs B1.)

## Input suppression (data-driven hold mask)

B1 hardcoded `GP_SELECT` in `input_state_cb`. B2 makes suppression follow the
bound holds:

- New global `unsigned g_hotkey_hold_mask` in `callbacks.cpp` (next to
  `g_map0`/`g_map1`), default `GP_SELECT`.
- `input_state_cb`: `if (port == 0 && (buttons & g_hotkey_hold_mask)) return 0;`
  (replaces the B1 `GP_SELECT` literal).
- The kernel sets `g_hotkey_hold_mask = OR over i of pad_bit(hk.b[i].hold)` at boot
  (after loading settings) and after any remap-screen change. With the default
  bindings this equals `GP_SELECT` — exactly B1's behavior. Only buttons actually
  bound as a hold are masked.

## Remap UI — `HotkeyScreen`

New sub-screen `src/menu/hotkey_screen.{h,cpp}`, mirroring `ControlsScreen`
(UP/DOWN select row, LEFT/RIGHT cycle the row's value, B backs out), reached from
a new `Hotkeys...` row on the Settings screen.

- **Layout — two rows per action (12 rows)**, faithful to ControlsScreen's
  one-cyclable-value-per-row idiom:
  ```
  Quick-save  hold: < Select >
  Quick-save  key:  < X >
  Quick-load  hold: < Select >
  Quick-load  key:  < Y >
  ... (Vol+, Vol-, HUD, Mute)
  ```
  - "hold" rows cycle {Select, Start, L, R}.
  - "key" rows cycle the `PadButton` set, **skipping the value equal to that
    action's current hold** (so `hold == trigger` can't be selected). If changing
    a hold would equal the current trigger, the trigger advances to the next valid
    value.
- **Conflict indicator:** a pure `unsigned hotkey_conflicts(const HotkeyBindings&,
  MenuHotkey menu)` returns a per-action conflict bitset — set when an action's
  combo duplicates another action's combo, or equals the `menu_hotkey` preset
  mask. Conflicting rows render a trailing `!`. Runtime decode resolves duplicates
  by array priority, so a flag is informative, not fatal.
- **Persistence + live:** each change calls `m_pStore->Save(*m_pSettings)` and
  recomputes `g_hotkey_hold_mask` (extern), so edits apply immediately on return
  to the game.
- **Settings wiring:** `SettingsScreen` gains a `HotkeyScreen*` dependency and a
  `Hotkeys...` nav row (`NUM_ROWS` 12 → 13; START opens it, handled like the
  existing Controls/Video Mode nav rows). Constructed in the kernel and passed in,
  exactly like `ControlsScreen`/`VideoModeScreen`.

## Kernel wiring

- Build/hold the `HotkeyBindings` in `m_Settings` (already loaded by
  `SettingsStore`); pass `m_Settings.<hotkeys>` to `decode_hotkey` in the play
  loop (replacing the 2-arg call).
- Compute `g_hotkey_hold_mask` at boot after settings load.
- Construct `m_HotkeyScreen` and pass it to `m_SettingsScreen`.

## Testing

- **Host — generalized `decode_hotkey`** (`test/test_hotkey.cpp`): with custom
  `HotkeyBindings`, each action fires for its bound combo; **priority** resolution
  when two actions share a combo (lower index wins); hold-not-held and
  trigger-not-pressed → `None`; the **default** bindings satisfy the no-menu-
  collision invariant for every `menu_hotkey` preset.
- **Host — `pad_button_from_token` + binding round-trip** (`test/test_settings.cpp`):
  each `hotkey_*` key round-trips; malformed / `hold == trigger` / out-of-set hold
  → that action's default; defaults parse to the expected combos.
- **Host — `hotkey_conflicts`** (`test/test_hotkey.cpp`): no conflicts on defaults;
  a duplicated combo flags both actions; a combo equal to each `menu_hotkey` preset
  flags that action.
- **Device build (`make`)**: `HotkeyScreen`, the Settings nav row, the
  `g_hotkey_hold_mask` global + `input_state_cb` use, and the kernel wiring all
  compile.
- No host test for `HotkeyScreen` rendering (device-only; consistent with the
  other menu screens).

## Hardware verification (checklist section O)

- Open Settings → `Hotkeys...`; remap an action's hold and/or key; confirm the new
  combo performs the action in-game and the old one no longer does.
- The binding persists across reboot (`hotkey_*` keys in `SD:/settings.txt`).
- Setting a duplicate combo shows `!` on both rows; the higher-priority action
  still works (priority resolution).
- Changing an action's hold to a different safe button: suppression now also masks
  player-1 input while that button is held; Select-held still works if any action
  uses it.
- Defaults (fresh `settings.txt`) match: Select + X/Y/A/B and Select + L/R for
  volume.

## Cross-references

- Spec B1 (fixed hotkeys + toasts): `docs/superpowers/specs/2026-06-21-ingame-hotkeys-toasts-design.md`
- ControlsScreen remap precedent: `src/menu/controls_screen.{h,cpp}`
- Button helpers: `src/input/joypad_map.{h,cpp}` (`pad_bit`), `src/settings/settings.cpp` (`pad_button_token`, `parse_button_map`)
- Deferred enhancements #8: `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md`
