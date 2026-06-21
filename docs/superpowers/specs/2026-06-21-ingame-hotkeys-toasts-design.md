# In-Game Hotkeys + Toasts — Design (Spec B1)

**Status:** approved design — ready for implementation plan.
**Date:** 2026-06-21
**Completes deferred video enhancement #8** (on-screen overlay) by adding the
action/feedback layer on top of the Spec A overlay
(`docs/superpowers/specs/2026-06-21-overlay-hud-design.md`).

This is **Spec B1**. Spec B2 (remappable hotkey bindings + capture UI, persisted)
is deferred and will build on the pure `decode_hotkey` introduced here.

## Problem

Spec A added an overlay layer and a diagnostics HUD, but every user action
(save/load, volume, mute, HUD) still requires opening the pause menu. There is no
way to act mid-game, and no transient on-screen feedback ("toast") for actions.

## Goal

Add **in-game hotkeys** for the common actions and **transient toast notices**
that confirm them, reusing the Spec A `Overlay`/`TextCanvas` layer. Hotkeys use a
fixed **Select + button** mapping (modifier + action), chosen so they never
collide with any `menu_hotkey` preset.

Non-goals (deferred to Spec B2): remappable/configurable bindings, a binding
capture UI, per-action persistence of bindings, a toast queue.

## Hotkey mapping (fixed)

Modifier = **Select held** on player 1; action = a fresh press of the second
button:

```
Select + X      -> Quick-save        Select + Up    -> Volume up
Select + Y      -> Quick-load        Select + Down  -> Volume down
Select + A      -> Toggle HUD        Select + B     -> Mute toggle
```

This avoids all four `menu_hotkey` presets (Start+Select, Start+A, Start+B, L+R)
by construction: none of them is a *Select + face/d-pad* combo.

## Architecture

Three bounded units plus a 2-line input guard:

### 1. Pure hotkey decoder — `src/input/hotkey.{h,cpp}`

Host-testable, no Circle dependency. Isolates the mapping so Spec B2 can later
generalize it.

```cpp
enum class InGameAction { None, QuickSave, QuickLoad, VolUp, VolDown,
                          ToggleHud, Mute };

// held    = current GP_* bitmask (this frame)
// pressed = newly-pressed edge bits (this frame)
// Returns the action when Select is in `held` and the mapped action button is in
// `pressed`; otherwise None. (Uses GP_* bits from joypad_map.h.)
InGameAction decode_hotkey(unsigned held, unsigned pressed);
```

Mapping inside `decode_hotkey`: requires `held & GP_SELECT`, then matches
`pressed` against `GP_X`/`GP_Y`/`GP_A`/`GP_B`/`GP_UP`/`GP_DOWN` in that priority
order, returning the corresponding action; else `None`.

### 2. Toast channel on `Overlay` — `src/ui/overlay.{h,cpp}`

A second channel independent of the HUD `enabled` flag, so toasts show even when
the diagnostics HUD is off.

```cpp
// new members
char     m_Toast[TOAST_MAX + 1];   // TOAST_MAX = 28
unsigned m_ToastFrames;            // countdown; 0 = inactive

void ShowToast(const char *msg);   // copy (truncated), m_ToastFrames = TOAST_FRAMES
void DrawToast(void);              // every frame: if active, draw then decrement
```

- `TOAST_FRAMES = 120` (~2 s at 60 fps).
- **Position:** bottom-center opaque box (the HUD is top-left — never overlap).
  Box width = message length x `CharW` + padding, centered via `TextCanvas`
  cols/rows (`Cols()*CharW()` = framebuffer width).
- **Lifetime:** `ShowToast` overwrites any current message and resets the
  countdown (latest action wins; no queue). When the countdown reaches 0 it stops
  drawing; because the toast sits over the centered game raster, the next `Blit`
  repaints those pixels — no `ForceRepaint` needed. Same no-ghost, redraw-each-
  frame model as the HUD across the two vsync pages.
- Fixed-size buffer; truncates to `TOAST_MAX`; no heap.
- The existing `Draw(const HudStats&)` (HUD) is unchanged and still gated on
  `Enabled()`.

### 3. Action dispatch — kernel play loop (`src/kernel.cpp`)

After polling input (where `held`/`pressed` are already computed for the menu
hotkey), call `decode_hotkey(held, pressed)` and execute, reusing existing live
paths. Each action shows a toast:

| Action    | Effect                                                  | Toast |
|-----------|---------------------------------------------------------|-------|
| QuickSave | `m_SaveState.Save(1)`                                    | `Quick-saved` / `Save failed` |
| QuickLoad | if `m_SaveState.Occupied(1)` → `Load(1)`, else no-op    | `Quick-loaded` / `No quick save` / `Load failed` |
| VolUp     | `volume = min(100, volume+10)`, `SetVolume`, persist     | `Volume NN` |
| VolDown   | `volume = max(0, volume-10)`, `SetVolume`, persist       | `Volume NN` |
| ToggleHud | flip `debug_overlay`, `m_Overlay.SetEnabled`, persist    | `HUD on` / `HUD off` |
| Mute      | flip `mute`, `m_Audio.SetMute`, persist                  | `Muted` / `Unmuted` |

- "persist" = `m_SettingsStore.Save(m_Settings)` (same as the settings screen), so
  in-game volume/mute/HUD tweaks survive reboot. Quick-save/load do not touch
  settings.
- Quick actions use the dedicated **slot 1**; `QuickLoad` on an empty slot 1 is a
  no-op (no core call) with a `No quick save` toast.
- Toast strings are built in the kernel; the volume number uses the existing
  manual integer formatting style (no `snprintf`).
- The menu-hotkey check is unchanged and runs as today; `decode_hotkey` is
  evaluated on the same frame's `held`/`pressed`.

### 4. Input suppression — `src/libretro/callbacks.cpp`

Make Select a clean "hotkey mode" for player 1 so combos don't leak into the
game:

```cpp
unsigned buttons = g_gamepad->Buttons(port);
if (port == 0 && (buttons & GP_SELECT))   // player-1 hotkey mode: mask input
    return 0;
return joypad_state(buttons, id, *map);
```

- Masks **only port 0**, and only while *its* Select is held — player 2 is
  unaffected.
- Suppresses the whole player-1 pad that frame, so neither Select nor the action
  button (nor an accidental D-pad press) reaches the core during a combo.
- `GP_SELECT` is already available via `joypad_map.h` (included by
  `callbacks.cpp`); no new dependency or shared global.
- Documented trade-off: a game using the Select/Mode button alone won't receive
  it while held.

## Testing

- **Host — `decode_hotkey`** (`test/test_hotkey.cpp`, added to `test/Makefile`):
  - Each of the six combos returns its action when Select is in `held` and the
    action button is in `pressed`.
  - Edge semantics: action only in `held` (not `pressed`) → `None`; action
    pressed without Select held → `None`.
  - **No menu-hotkey collision (invariant):** for each `menu_hotkey` preset mask
    (Start+Select, Start+A, Start+B, L+R) fed as both `held` and `pressed`,
    `decode_hotkey` returns `None`.
- **Device build (`make`)**: toast members/methods on `Overlay`, the kernel
  dispatch, and the `input_state_cb` guard all compile.
- No host test for toast rendering or suppression (device-only; consistent with
  how `Overlay`/menus are handled).

## Hardware verification (checklist section N)

- Each combo performs its action and shows the correct toast (bottom-center).
- `Select + Y` (quick-load) on an empty slot 1 shows `No quick save`; after a
  `Select + X` quick-save it loads.
- Volume up/down change audio live and the toast shows the new level; the value
  persists across reboot.
- Toggle HUD and Mute work in-game and persist across reboot.
- While Select is held, the game receives no player-1 input (suppression);
  releasing Select restores normal play. Player 2 is unaffected.
- Toast disappears after ~2 s with no ghost text left over the game.

## Cross-references

- Spec A (overlay layer + HUD): `docs/superpowers/specs/2026-06-21-overlay-hud-design.md`
- Deferred enhancements #8: `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md`
- Hotkey bits + presets: `src/input/joypad_map.{h,cpp}` (`GP_*`, `hotkey_mask`, `MenuHotkey`)
- Save slots: `src/menu/save_state.h` (`Save`/`Load`/`Occupied`, slots 1..4)
- Spec B2 (follow-on): remappable bindings + capture UI, persisted (to be written).
