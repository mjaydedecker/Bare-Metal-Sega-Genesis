# Controller Remapping — Design

**Date:** 2026-06-20
**Status:** approved design, pending implementation plan
**Related:** FSD §4.9.2 (`controller_1_map` / `controller_2_map`), §4.6 (controller input); the settings subsystem + Settings screen; two-controller support; M7 input.

## Summary

Let the user remap the 8 Genesis action buttons (A, B, C, X, Y, Z, Start, Mode)
to physical USB-pad buttons, independently for Player 1 and Player 2, via a
"Controls" sub-screen reached from the Settings screen. Maps persist in
`SD:/settings.txt` as `controller_1_map` / `controller_2_map`. The D-pad is not
remappable.

## Background / Key Fact

The core's Genesis←libretro-id mapping is **fixed** (`libretro.c` ~463–490):
Genesis A←`JOYPAD_Y`, B←`JOYPAD_B`, C←`JOYPAD_A`, X←`JOYPAD_L`, Y←`JOYPAD_X`,
Z←`JOYPAD_R`, Start←`JOYPAD_START`, Mode←`JOYPAD_SELECT`; D-pad←`JOYPAD_UP/DOWN/
LEFT/RIGHT`. All remapping therefore lives in **our** `joypad_state`, which today
hardcodes physical `GP_*` → libretro id. This design makes that table-driven.

## Data Model (pure, in `settings.h`)

- `enum class PadButton { A, B, X, Y, L, R, Start, Select };` — the 8 assignable
  physical pad buttons.
- `struct ButtonMap { PadButton b[8]; };` — indexed by Genesis button in the
  fixed order **0=A, 1=B, 2=C, 3=X, 4=Y, 5=Z, 6=Start, 7=Mode**. `b[i]` is the
  physical button that drives Genesis button `i`.
- `Settings` gains `ButtonMap map1, map2;` (P1/P2).
- **Default map** (reproduces today's behavior exactly):
  A→Y, B→B, C→A, X→L, Y→X, Z→R, Start→Start, Mode→Select. A
  `default_button_map()` helper (or in-struct init) supplies it; both `map1` and
  `map2` default to it.
- The D-pad is fixed (directional) — never in `ButtonMap`.

These types are pure (no Circle / libretro). `joypad_map.h` already includes
`settings.h`, so the dependency direction stays one-way (`joypad_map → settings`).

## `joypad_map` Refactor

- `unsigned pad_bit(PadButton p)` → the `GP_*` bit (`A→GP_A`, `B→GP_B`, `X→GP_X`,
  `Y→GP_Y`, `L→GP_LB`, `R→GP_RB`, `Start→GP_START`, `Select→GP_SELECT`).
- `int16_t joypad_state(unsigned buttons, unsigned retro_id, const ButtonMap &map)`:
  - For the 8 action libretro ids, map id → Genesis index (fixed table here),
    look up `map.b[index]`, test `buttons & pad_bit(...)`.
  - D-pad ids (`UP/DOWN/LEFT/RIGHT`) test their fixed `GP_*` (unchanged).
  - `RETRO_DEVICE_ID_JOYPAD_MASK` builds the bitmask by calling itself per id,
    passing `map` through.
  - Unknown ids → 0.
- `hotkey_mask` (existing) is unchanged.

## Controls Sub-Screen (`src/menu/controls_screen.{h,cpp}`)

A dedicated TextCanvas screen, same cycle-row pattern as the Settings screen,
reached from a Settings-screen `Controls...` row (opened by pressing **Start** on
that row — it is an action, not a value-cycler).

Rows:
- `Player: < 1 | 2 >` — selects which map (`map1`/`map2`) is being edited.
- 8 rows, one per Genesis button:
  `A: < Y >`, `B: < B >`, `C: < A >`, `X: < L >`, `Y: < X >`, `Z: < R >`,
  `Start: < Start >`, `Mode: < Select >`. Left/right cycles the physical button
  through the 8 `PadButton` values.
- **B** = back to the Settings screen.

Behavior: up/down moves between the Player row and the 8 button rows; left/right
changes the focused row's value. Each change updates the selected `ButtonMap` in
`m_Settings` and persists via `SettingsStore::Save` — live (the core reads the
map on the next frame). Conflicts (two Genesis buttons sharing one physical
button) are allowed and visible, not blocked.

Constructor takes `TextCanvas*`, `Gamepad*`, `CUSBHCIDevice*`, `Settings*`,
`SettingsStore*`.

## Storage

Two keys, comma-encoded in Genesis-button order (A,B,C,X,Y,Z,Start,Mode), each
token a physical button (`a` | `b` | `x` | `y` | `l` | `r` | `start` | `select`):

```
controller_1_map=y,b,a,l,x,r,start,select
controller_2_map=y,b,a,l,x,r,start,select
```

- Pure helpers: `const char *pad_button_token(PadButton)` and a parser that turns
  a comma string into a `ButtonMap`.
- Parse: split into 8 tokens; each → `PadButton`. An unrecognized token or a
  wrong token count → the whole map falls back to the default (never a partial /
  broken map).
- Serialize: emit the 8 tokens for `map1` and `map2`.

## Wiring

- `callbacks.cpp`: `extern const ButtonMap *g_map0, *g_map1;`. `input_state_cb`
  picks `port == 0 ? g_map0 : g_map1` and calls
  `joypad_state(g_gamepad->Buttons(port), id, *map)`. (Both default to a static
  default map until the kernel sets them, so a null is never dereferenced.)
- `kernel.cpp`: at boot, after loading settings, set
  `g_map0 = &m_Settings.map1; g_map1 = &m_Settings.map2;` (pointers stay valid;
  the sub-screen edits the contents live). Own a `ControlsScreen`; the Settings
  screen opens it.
- `settings_screen`: add the `Controls...` row that opens the `ControlsScreen`
  (needs a `ControlsScreen*`).

## Error Handling

- Malformed / short / unknown map value → default map; never empty or partial.
- Conflicts allowed; both Genesis buttons fire from the shared physical button.
- D-pad always works regardless of the map.
- `g_map0`/`g_map1` default to a static default `ButtonMap` so input works even
  before the kernel wires them.

## Testing

**Host unit tests:**
- `pad_bit` for all 8 `PadButton` values → expected `GP_*`.
- Map parse: a valid 8-token string round-trips; a bad token → default; a
  wrong-count string → default; serialize→parse equals the original.
- `joypad_state` with the **default** map still passes the existing per-id
  assertions (regression guard), and with a **remapped** map (e.g. swap so
  physical A drives Genesis A via `map.b[0]=PadButton::A`) returns the remapped
  bits; `MASK` honors the map.

**Hardware verification:**
- Remap P1 (e.g. Genesis A → physical A), confirm in a game.
- Switch the sub-screen to P2, set a different map, confirm P1 and P2 are
  independent in a 2-player game.
- Reboot → both maps persist in `SD:/settings.txt`.

## Out of Scope

- Remapping the D-pad or analog axes.
- Per-physical-controller auto-detection / profiles (maps are per *port*).
- Blocking or auto-resolving conflicts.
- This completes the FSD §4.9 config table (`controller_1_map`/`controller_2_map`
  were the last unimplemented keys).
