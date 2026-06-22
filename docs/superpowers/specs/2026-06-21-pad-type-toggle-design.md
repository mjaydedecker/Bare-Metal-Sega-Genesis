# 3/6-Button Pad Type Toggle — Design

**Date:** 2026-06-21
**Status:** approved design, pending implementation plan
**Related:** controller deferred item "3/6-button toggle" ([[project-two-controllers]]);
the settings subsystem + Settings screen; `src/kernel.cpp` controller-port device
setup; the per-port `ButtonMap` remapping (unchanged by this).

## Summary

Let the user choose whether the emulated Genesis controllers present as
**3-button** or **6-button** pads, via a global `pad_type` setting in
`SD:/settings.txt` chosen on the Settings screen. The kernel currently hardcodes
6-button on both ports; a few Genesis titles misbehave when they detect a
6-button pad (the extra Mode button / 6-button handshake), so a 3-button option is
a compatibility fix. Default stays **6-button** (current behavior). Applies on the
next ROM load (the device is set each time a game loads), like `region`.

## Decisions (from brainstorming)

- **Global**, not per-port — one `pad_type` drives both ports (YAGNI; the
  real-world case is "this game needs 3-button," affecting both).
- **Applies on ROM reload**, not live — reuses the existing per-ROM-load device
  set; no per-frame device re-set logic.
- **Default 6-button** — no behavior change unless the user opts in.

## Background / Constraint

The core defines the device subclasses (verified in
`libs/genesis-plus-gx-wide/libretro/libretro.c:59-60`):

```c
#define RETRO_DEVICE_MDPAD_3B  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_MDPAD_6B  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
```

The kernel already defines `RETRO_DEVICE_MDPAD_6B` (`kernel.cpp:192`) and sets it
on both ports once per ROM load (`kernel.cpp:267-268`):

```c
retro_set_controller_port_device (0, RETRO_DEVICE_MDPAD_6B);
retro_set_controller_port_device (1, RETRO_DEVICE_MDPAD_6B);
```

So switching to 3-button is selecting subclass 0 instead of 1 at that point. The
physical USB pad and our `ButtonMap` are unaffected: in 3-button mode the core
simply ignores the X/Y/Z/Mode inputs.

## Settings Model (`src/settings/settings.{h,cpp}`)

- New enum (with the other setting enums in `settings.h`):
  ```cpp
  enum class PadType { ThreeButton, SixButton };
  ```
- New `Settings` field: `PadType pad_type;`, initialized to `PadType::SixButton`
  in the constructor's initializer list.
- File key `pad_type`, values `3button | 6button`. Parse in `parse_settings`
  (mirroring the `region` / `menu_hotkey` branches):
  ```cpp
  else if (ieq(key, "pad_type"))
      s.pad_type = ieq(val, "3button") ? PadType::ThreeButton
                                        : PadType::SixButton;
  ```
  (Unknown/missing → `SixButton`.)
- `serialize_settings` writes the key in the controller/input group:
  ```cpp
  appendz(out, out_size, "\npad_type=");
  appendz(out, out_size, pad_type_file_value(s.pad_type));
  ```
- Pure helper (declared in `settings.h`, defined in `settings.cpp` next to
  `region_file_value` / `menu_hotkey_file_value`):
  ```cpp
  // PadType as written to the settings file ("3button" | "6button").
  const char *pad_type_file_value(PadType p);
  ```
  with:
  ```cpp
  const char *pad_type_file_value(PadType p)
  {
      return p == PadType::ThreeButton ? "3button" : "6button";
  }
  ```

## Kernel Device Selection (`src/kernel.cpp`)

- Add the 3-button constant next to the existing 6B define (near line 192):
  ```cpp
  #define RETRO_DEVICE_MDPAD_3B RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
  ```
- Replace the hardcoded device set (lines 267-268) with a setting-driven choice:
  ```cpp
  unsigned padDev = (m_Settings.pad_type == PadType::ThreeButton)
                    ? RETRO_DEVICE_MDPAD_3B : RETRO_DEVICE_MDPAD_6B;
  retro_set_controller_port_device (0, padDev);
  retro_set_controller_port_device (1, padDev);
  ```

This block runs once per ROM load, so changing `pad_type` takes effect on the next
load. No `Apply()` hook and no global is needed — the kernel reads
`m_Settings.pad_type` directly. `input_state_cb` is unchanged (it already serves
ports 0/1 via the `ButtonMap`; the core decides which buttons a 3-button pad
exposes).

## Settings Screen (`src/menu/settings_screen.cpp`)

Add one cycle row, `Pad Type: < 6-button | 3-button >`, following the existing
toggle-row pattern (e.g. the Audio out / Vsync rows):

- Display value from `m_pSettings->pad_type`
  (`"< 6-button >"` / `"< 3-button >"`).
- Left/Right toggles the enum, writes `m_pSettings->pad_type`, persists via
  `m_pStore->Save(*m_pSettings)`. **No `Apply()` call** — the kernel reads it at
  the next ROM load.
- Insert the row at index 11 (after the Audio Latency row), shifting the action
  rows Controls/Video Mode/Hotkeys from 11/12/13 to 12/13/14, and bump
  `NUM_ROWS` 14 → 15. Update the `labels`/`values` arrays and the GP_START
  action-index checks accordingly. (The implementation plan resolves the exact
  indices against the current file.)
- Extend the footer hint to note the reload requirement, e.g. add `Pad: reload.`
  alongside the existing `Region: reload.` text.

## Error Handling

- Parse: unknown/missing `pad_type` → `SixButton` (safe default = today's
  behavior).
- The enum is always one of two valid values; the device select always yields a
  valid subclass. No invalid state is representable.

## Testing

**Host unit tests** (`test/test_settings.cpp`, extended):
- Default: `Settings().pad_type == PadType::SixButton`.
- Parse: `pad_type=3button` → `ThreeButton`; `pad_type=6button` → `SixButton`;
  `pad_type=bogus` and a missing key → `SixButton`.
- `pad_type_file_value`: `ThreeButton` → `"3button"`, `SixButton` → `"6button"`.
- Round-trip: set `ThreeButton`, `serialize_settings`, `parse_settings`, assert it
  survives.

(The kernel device select and the Settings-screen row are exercised on hardware,
consistent with the rest of `kernel.cpp` / the menu screens. Build verification is
the repo-root `make`.)

**Hardware verification** (new checklist item):
- Set Pad Type = 3-button, reload a game (or power-cycle). On a title that
  misbehaves with a 6-button pad, confirm it now plays correctly; on a
  6-button-aware game, confirm the X/Y/Z/Mode buttons no longer register.
- Set back to 6-button, reload → the 6-button buttons work again.
- Confirm `pad_type` is written to `SD:/settings.txt` and survives a reboot.

## Out of Scope

- **Per-port** pad type (one global setting only).
- **Live** (non-reload) switching.
- The **WAYPLAY / TEAMPLAYER** 3B/6B multitap device variants — that belongs to
  the separate deferred "multitap / 4+ players" item.
- Any `ButtonMap` change (the maps already cover all 8 buttons; the core gates
  X/Y/Z/Mode by device type).

## Cross-references

- Core device subclasses —
  `libs/genesis-plus-gx-wide/libretro/libretro.c:59-66`.
- Two-controller input layer + the deferred 3/6-button item —
  [[project-two-controllers]].
- FSD §4.9 (Configuration table) —
  `Documents/Bare-Metal-Sega-Genesis-FSD.md`.
</content>
