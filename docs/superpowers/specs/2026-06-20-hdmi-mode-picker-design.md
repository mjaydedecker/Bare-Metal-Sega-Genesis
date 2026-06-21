# Output HDMI Mode Picker — Design

**Date:** 2026-06-20
**Status:** IMPLEMENTED (2026-06-20) — `Display::SetMode` (67c37c1), `VideoModeScreen` confirm-or-revert (b898f37), kernel boot-apply + Settings "Video Mode..." row; host tests in `test_settings`. Pending hardware verification only.
**Related:** video settings spec (deferred this as "spec-2"); M5 video output (the
framebuffer=output-mode gotcha); the settings subsystem + Settings screen.

## Summary

Let the user choose the HDMI output resolution from a fixed list
(Native / 1080p / 720p / 480p), guarded by a **confirm-or-revert** flow so a mode
the TV can't display never sticks. Only a visually-confirmed mode is persisted, so
boot is always safe. Stored as `video_mode` in `SD:/settings.txt`.

## Background / Constraint (the whole reason for the safety model)

On the Pi mailbox framebuffer the **physical framebuffer size IS the HDMI output
mode** (M5 lesson). A successful `CBcmFrameBuffer::Initialize()` does **not** prove
the TV accepts the signal — an unsupported mode yields a black "unsupported
signal" screen the user can't navigate. Software cannot reliably detect TV
rejection. Therefore the UI applies a new mode, shows a timed confirmation, and
**auto-reverts** if the user doesn't confirm; and the setting is **persisted only
after confirmation**, guaranteeing every saved mode displayed at least once.

## Model & Mapping

- `enum class VideoMode { Native, P1080, P720, P480 };` in `Settings`; field
  `video_mode`, default `Native`. File key `video_mode=native|1080p|720p|480p`.
- Pure helper `void video_mode_dims(VideoMode m, unsigned &w, unsigned &h)`:
  - `Native → (0, 0)` (firmware's current/native mode, as today)
  - `P1080 → (1920, 1080)`
  - `P720 → (1280, 720)`
  - `P480 → (720, 480)`
- Pure `const char *video_mode_file_value(VideoMode)` → keyword, and parse
  (unknown → `Native`), mirroring `region`/`menu_hotkey`.

## `Display::SetMode`

Add `bool Display::SetMode(unsigned w, unsigned h)`:
- Delete the current `CBcmFrameBuffer`, create `new CBcmFrameBuffer(w, h, FB_DEPTH)`
  (w=h=0 → native), `Initialize()`, and on success update `m_pBuffer`, `m_Pitch`,
  `m_FbW`, `m_FbH`, reset `m_LastW/m_LastH`, and `ClearBlack()`. Return true.
- **On failure** (allocation or `Initialize()` returns false / null buffer),
  recreate the framebuffer at the previous `(w,h)` and return false — the caller
  stays on the prior mode.
- `TextCanvas` and `Blit` already re-read `Buffer()/Pitch()/Width()/Height()`
  each call, so they adopt the new framebuffer automatically. `Blit`'s
  integer/stretch centering recomputes from the new `m_FbW/m_FbH`.

`Display::Initialize()` is unchanged (still boots at native); the saved mode is
applied right after settings load.

## Confirm-Revert Flow — `VideoModeScreen` (`src/menu/video_mode_screen.{h,cpp}`)

A small TextCanvas screen, same pattern as `ControlsScreen`, reached from a
Settings-screen `Video Mode...` row (opened with **Start**).

- Shows the active mode; **left/right selects** a candidate from the four (no
  signal change while selecting).
- **Start = apply:**
  1. Record the current known-good `VideoMode` (and its dims).
  2. `Display::SetMode(candidate dims)`. If it returns false, show a brief
     "mode unavailable" message and stay; don't save.
  3. Otherwise draw a countdown overlay on the **new** framebuffer:
     **"Keep this mode? A = keep   Reverting in N…"**, ~15 s, decrementing.
     - **A** → persist `video_mode = candidate` via `SettingsStore::Save`; return.
     - **B or timeout** → `Display::SetMode(previous dims)` and do **not** save.
- **B** (when not in a countdown) = back to the Settings screen.

Constructor: `VideoModeScreen(TextCanvas*, Gamepad*, CUSBHCIDevice*, Settings*,
SettingsStore*, Display*)`. Navigation uses `Gamepad::MenuButtons()` (either pad).

## Settings Screen + Kernel Wiring

- `SettingsScreen` gains a `VideoModeScreen *pVideoMode` (forward-declared, like
  `ControlsScreen`). A new `Video Mode...` action row opens it on **Start**.
- `kernel.cpp`: own a `VideoModeScreen`; pass `&m_VideoModeScreen` to
  `SettingsScreen`. After settings load, apply the saved mode:
  ```
  if (m_Settings.video_mode != VideoMode::Native) {
      unsigned w, h; video_mode_dims(m_Settings.video_mode, w, h);
      m_Display.SetMode(w, h);   // reverts to native + logs on failure
  }
  ```

## Error Handling

- `SetMode` failure → revert to prior framebuffer, return false; caller unaffected.
- Boot apply failure → native fallback, serial log; never a crash or black boot.
- Confirm timeout / B → revert + no save (the core safety).
- Unknown `video_mode` value on parse → `Native`.
- Residual risk: a persisted mode that *initializes* but a *different* TV can't
  display. Mitigated by the boot path falling back to native on `SetMode`
  failure; the rare "initializes but TV-blank on another panel" case is accepted.

## Testing

**Host unit tests:**
- `video_mode_dims` for all four values (Native→0,0; 1080p→1920,1080;
  720p→1280,720; 480p→720,480).
- `video_mode` keyword parse (each value; invalid/missing → Native) and
  serialize round-trip (extend `test_settings`).

(The framebuffer reallocation + confirm-revert flow is Circle/hardware and is
verified on hardware, like the rest of `Display`.)

**Hardware verification:**
- Settings → Video Mode... → select 720p → Start → confirm prompt shows → A keeps
  it; reboot → still 720p.
- Select a mode the TV rejects → black → wait out the countdown → the screen
  returns to the previous mode; nothing was saved.
- Confirm the game image still centers/scales correctly at each resolution
  (integer and stretch modes).

## Out of Scope

- EDID enumeration of TV-supported modes (fixed candidate list instead).
- Arbitrary/custom resolutions or refresh-rate selection.
- Separate per-mode scaling tweaks (the existing `video_scale` applies).
