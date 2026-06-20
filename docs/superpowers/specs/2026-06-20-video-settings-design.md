# Video Settings — Design (Spec 1 of 2)

**Date:** 2026-06-20
**Status:** approved design, pending implementation plan
**Related:** FSD §4.9 (Configuration), `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md` (items #4 scaling modes, #5 widescreen, #7 config-driven settings)

## Summary

Introduce a settings subsystem for the emulator and use it to make two video
options user-configurable: **video scaling mode** (integer vs stretch) and
**widescreen** (Genesis-Plus-GX-Wide extended horizontal columns). Settings are
stored in a dedicated file on the SD card, parsed at boot, and editable via a new
in-emulation Settings screen that writes them back.

This is **spec 1 of 2**. Output HDMI mode selection is deliberately deferred to a
follow-on spec (see Scope below).

## Scope & Decomposition

**In this spec:**
- A reusable `Settings` module (typed struct + pure parse/serialize + file I/O).
- `video_scale` setting (enum: `integer` | `stretch`), applied live.
- `widescreen` setting (bool), applied on next ROM load / reset.
- A Settings screen reached from the existing pause menu.

**Deferred to a follow-on spec — output HDMI mode selection.** Rationale: on the
Pi mailbox framebuffer the physical framebuffer size *is* the HDMI output mode
(the M5 hardware lesson — forcing an unsupported mode produces "unsupported
signal" / a black screen). A user-facing resolution picker must enumerate
firmware-supported modes, apply a safe fallback, and re-allocate the framebuffer
(possibly mid-game, with the Approach-B flicker concern). A setting that can
black out the display is the worst one to introduce before the settings plumbing
is proven. It gets its own focused spec once this foundation lands.

The settings file format reserves room for the other FSD §4.9 keys
(`region`, `auto_launch_rom`, `audio_*`, controller maps, `menu_hotkey`), but
only `video_scale` and `widescreen` are wired up here.

## Architecture

### New module: `src/settings/`

Follows the project's established pattern of separating host-testable pure logic
from Circle-dependent I/O (cf. `src/menu/save_path`, `src/menu/sram`).

- **`settings.h` / `settings.cpp`** — pure, no Circle dependencies:
  - `struct Settings` with typed fields and default values.
  - `Settings parse_settings(const char *text)` — parse `key=value` text into a
    `Settings`, falling back to defaults for missing/invalid/unknown keys.
  - `void serialize_settings(const Settings&, char *out, size_t out_size)` —
    render a `Settings` back to `key=value` text.
  - Host-unit-tested in `test/`.

- **Load/save I/O** — a thin layer (in the kernel or a small `settings_store`
  helper) using the existing `Storage` helpers (`Exists`, `ReadFile`,
  `WriteFile`) to read `SD:/settings.txt` at boot and write it back on change.
  If the file is absent, all defaults are used and a default file is written out.

### File format

Plain text, one setting per line:

```
# Bare Metal Sega Genesis settings
video_scale=integer
widescreen=off
```

- `key=value`, one per line.
- Lines beginning with `#` are comments.
- Whitespace around keys/values trimmed.
- Unknown keys are ignored on read (forward-compatibility) — a newer settings
  file opened by older firmware won't break, and vice versa.
- Stored at **`SD:/settings.txt`** — a dedicated file, **not** the firmware's
  `config.txt` (which the Pi GPU firmware reads at boot; writing to it risks
  corrupting the boot configuration).

### Ownership

`CKernel` owns one `Settings` instance, loaded at boot. It is passed by pointer
to the consumers that need it:
- `Display` (for `video_scale`),
- the libretro environment callback (for `widescreen`),
- the Settings screen (to read/edit and trigger write-back).

A pointer is preferred over a global singleton so the pure struct stays
injectable for host tests; this mirrors how existing globals
(`g_display`, `g_rom_data`) are wired while keeping the data testable.

## Settings & Defaults (this spec)

| Setting | Type | Default | Apply timing |
|---|---|---|---|
| `video_scale` | enum `integer` \| `stretch` | `integer` | Live (next frame) |
| `widescreen` | bool (`on`/`off`) | `off` | Next ROM load / reset |

## Integration Points

### `video_scale` — live

`Display::Blit` currently always integer-scales (largest whole factor) and
centers with black bars. We add a scale-mode the display reads from `Settings`:

- **`integer`** — existing path: nearest-neighbor whole-factor scale, centered,
  letterboxed. Unchanged behavior.
- **`stretch`** — aspect-corrected fill of the framebuffer's 4:3 area using
  independent horizontal/vertical scale factors (CPU blit) instead of a single
  integer factor.

`Display` is given the `Settings*` (or a `SetScaleMode()`) at init; changing the
mode affects only the next `Blit`, so it is instantly live with no framebuffer
re-allocation. The scaling math lives in the host-tested `src/video/blit.cpp` so
both modes are unit-tested.

### `widescreen` — apply on reset/load

The `RETRO_ENVIRONMENT_GET_VARIABLE` handler (`src/libretro/environment.cpp`,
currently hardcoding `genesis_plus_gx_wide_h40_extra_columns` to `"0"`) returns
the value from `Settings` instead:
- `widescreen=off` → `"0"` (native 320-wide).
- `widescreen=on` → the core's wide value (extra columns).

The core re-reads variables only at init/reset, so the Settings screen shows an
"applies on reset" note; the change takes effect on the next ROM load or reset.
`Blit` already adapts to the wider source frame automatically (it re-centers on
any frame-size change), so no further video changes are needed.

## Settings Screen UI

A new screen reached from the existing `PauseMenu` "Settings" entry, built on the
same `TextCanvas` framebuffer UI as the pause menu and ROM browser (drawn on the
game framebuffer, no console switching).

Rows:

```
Settings

  Video Scale:  < Integer | Stretch >
  Widescreen:   < Off | On >   (applies on reset)

  [B] Back
```

- D-pad up/down moves the selection between rows.
- D-pad left/right cycles the value of the selected row.
- B / Back returns to the pause menu.
- On each change (or on exit), `serialize_settings` →
  `Storage::WriteFile("SD:/settings.txt")`.
- `video_scale` changes update `Display` live; `widescreen` changes update the
  `Settings` the environment callback reads (effective next reset).

## Error Handling

- Missing `SD:/settings.txt` → use all defaults, write a default file.
- Unparseable / partial file → per-key fallback to default; never fail to boot.
- `WriteFile` failure on save → keep the in-memory change for the session, log to
  serial; do not crash. (Persistence is best-effort, consistent with the save
  subsystems.)

## Testing

**Host unit tests** (`test/`, following the existing host-test pattern):
- `parse_settings`: round-trip of valid input; defaults for missing keys;
  defaults for invalid values; unknown keys ignored; comments/whitespace handled.
- `serialize_settings`: output is parseable back to an equal `Settings`.
- `blit`: stretch-mode scaling math (alongside the existing integer-mode tests).

**Hardware verification:**
- Toggle Video Scale integer↔stretch and confirm the picture changes live.
- Toggle Widescreen on, reset, confirm wider rendering; toggle off + reset.
- Reboot and confirm `SD:/settings.txt` persisted the chosen values.

## Open Items / Future

- Output HDMI mode selection — follow-on spec (see Scope).
- Remaining FSD §4.9 settings (`region`, `auto_launch_rom`, audio, controller
  maps, `menu_hotkey`) — added to the `Settings` struct and screen as their
  owning features land.
