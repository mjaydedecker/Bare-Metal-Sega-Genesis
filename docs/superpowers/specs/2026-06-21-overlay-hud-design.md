# On-Screen Overlay Layer + Diagnostics HUD — Design (Spec A)

**Status:** approved design — ready for implementation plan.
**Date:** 2026-06-21
**Advances deferred video enhancement #8** (on-screen logging / debug overlay)
from `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md`.

This is **Spec A of two**. Spec B (in-game action hotkeys + transient toast
notices) builds on the overlay layer introduced here and will be brainstormed
separately.

## Problem

Diagnostics are invisible without hardware. The kernel emits audio
underrun/overrun counts to the **serial log every ~5 s**, but per the project's
own findings the log target is the serial UART (and historically the screen
console) — nothing useful reaches HDMI during gameplay. There is no on-screen way
to see whether the Pi is keeping up (frame rate) or how the audio buffer is
behaving. A developer needs a USB-TTL cable to see any of it.

## Goal

Add a thin **overlay layer** that composites text over the live game frame, and a
**diagnostics HUD** that uses it to show measured FPS, audio underruns/overruns,
audio queue depth, and the current ROM + video mode/scale. The HUD is gated by a
persisted, live `debug_overlay` setting (default off).

Non-goals (deferred to Spec B): transient toast notices, in-game action hotkeys
(quick-save, volume ±), a scrolling text console, configurable HUD position or
fields.

## Architecture

Three units, each with one responsibility:

1. **`hud_format` (pure, host-tested)** — `src/ui/hud.{h,cpp}`. Turns a
   `HudStats` struct into fixed-width text lines. No Circle dependency.
2. **`Overlay` (device)** — `src/ui/overlay.{h,cpp}`. Holds an enabled flag and a
   `TextCanvas *`; `Draw(const HudStats &)` calls `hud_format` then paints the box
   with `FillRect` + `DrawText`.
3. **Kernel wiring** — owns the `Overlay`, measures FPS, builds `HudStats` each
   frame, and calls `Overlay::Draw` after `retro_run()` when enabled.

### Compositing approach

Draw the HUD **after** the game frame, on the visible front page, via the
existing `TextCanvas` — touching none of the tuned vsync/pacing logic.

- In the kernel play loop, immediately after `retro_run()` returns, the game
  frame has been blitted and (if vsync is on) already page-flipped, so
  `Display::Buffer()` is the just-shown page. The kernel calls `m_Overlay.Draw()`
  there.
- The HUD is a **fixed-size, opaque-background text box** at the top-left of the
  framebuffer, redrawn in full every frame. Because `TextCanvas::DrawText` fills
  each cell's background and the box size is constant, there is **no ghosting**
  on either page and stale text from the alternate buffer is always overwritten.
- **No changes to `Display::Blit`/`Present`** — zero risk to the vsync/pacing
  work (checklist K). Trade-off: the HUD text is drawn just after the flip, so
  the overlay itself (not the game) may show minor tearing. Acceptable for a
  debug HUD; keeps the hot path untouched.
- Toggling the HUD **off** calls `Display::ForceRepaint()` once so any HUD pixels
  in the letterbox bars are cleared; pixels over the game raster are overwritten
  by the next `Blit` regardless.

## HUD content & formatting

```cpp
// src/ui/hud.h
#define HUD_COLS 22

struct HudStats {
    unsigned fps;            // measured frames/sec
    unsigned underruns;      // m_Audio.Underruns()
    unsigned overruns;       // m_Audio.Overruns()
    unsigned queued;         // m_Audio.QueuedFrames()
    unsigned target;         // pacing target depth
    const char *rom;         // base ROM name (may be long / NULL)
    const char *mode;        // "1080p" / "720p" / "Native" ...
    const char *scale;       // "integer" / "stretch" / "aspect"
};

// Fills up to max_lines NUL-terminated lines (each <= HUD_COLS chars + NUL).
// Returns the number of lines written.
unsigned hud_format(const HudStats &s, char lines[][HUD_COLS + 1],
                    unsigned max_lines);
```

Four-line layout (fixed columns, `HUD_COLS = 22`):

```
FPS 59  U 0  O 2
AQ 1440/2880
Sonic The Hedgehog
1080p  aspect
```

- The ROM line is **truncated** to `HUD_COLS` (long names never overflow). A NULL
  `rom` renders an empty/`"-"` line, not a crash.
- `mode`/`scale` come from current settings (mapped to short strings).
- **Measured FPS** is computed in the play loop: a frame counter plus a
  one-second window via `CTimer::GetClockTicks64()`; once per second
  `fps = frames_in_window`. Held in the kernel and passed into `HudStats` each
  frame (between recomputes the last value is shown).
- `queued`/`target` come from the audio pacing variables already in the loop.

## Settings wiring

Mirrors every existing setting (e.g. `mute`, `widescreen`, `vsync`):

- **`src/settings/settings.h`** — add `bool debug_overlay;`, default `false` in the
  constructor.
- **`src/settings/settings.cpp`** — parse `debug_overlay=on|off` via the existing
  `truthy()` helper; serialize it. New `settings.txt` key: `debug_overlay`.
- **`src/menu/settings_screen.cpp`** — add a `Debug Overlay: < On/Off >` row; the
  toggle flips `m_pSettings->debug_overlay` and applies it live via
  `m_Overlay.SetEnabled(...)` where the screen already applies other live
  settings. (The screen gains an `Overlay *` constructor dependency.)
- **Kernel** — constructs `Overlay m_Overlay(&m_Canvas)`; after loading settings
  at boot calls `m_Overlay.SetEnabled(m_Settings.debug_overlay)`. In the play
  loop, when enabled, builds `HudStats` and calls `m_Overlay.Draw(stats)` after
  `retro_run()`; on a live toggle to off, calls `Display::ForceRepaint()` once.
- **Default off** — opt-in; no behavior change unless enabled.

## Overlay interface

```cpp
// src/ui/overlay.h
class Overlay {
public:
    explicit Overlay(TextCanvas *pCanvas);
    void SetEnabled(bool on);          // live
    bool Enabled(void) const;
    void Draw(const HudStats &s);      // no-op if disabled
private:
    TextCanvas *m_pCanvas;
    bool        m_Enabled;
};
```

`Draw` is a no-op when disabled, so the loop can call it unconditionally if
preferred. When enabled it formats via `hud_format` and paints a background
`FillRect` plus one `DrawText` per line at the top-left, using a fixed cell size
from `TextCanvas::CharW/CharH`.

## Testing

- **Host — `hud_format`** (`test/test_hud.cpp`, added to `test/Makefile`):
  - All four fields land on the expected lines (FPS/U/O on line 0, queue on line
    1, ROM on line 2, mode+scale on line 3).
  - A ROM name longer than `HUD_COLS` is truncated; no write past `HUD_COLS+1`.
  - Returns the expected line count; every line is NUL-terminated and
    `<= HUD_COLS` chars.
  - Edge values (0 fps, 0 queue/target, NULL rom) format without garbage.
- **Host — settings round-trip** (`test/test_settings.cpp`): `debug_overlay=on`
  parses to `true` and re-serializes; default is `false`.
- **Device build**: `make` compiles `Overlay` + kernel wiring. No host test for
  the `TextCanvas` drawing (device-only, consistent with menus/Display).

## Hardware verification (checklist section M)

- `debug_overlay=on` selectable in Settings; applies live (HUD appears without
  reboot) and persists across reboot (`debug_overlay=on` in `SD:/settings.txt`).
- FPS reads ~60 on a game that keeps up; the audio U/O counters and AQ depth
  update live and match the periodic serial log.
- ROM name and mode/scale line are correct.
- Toggling off removes the HUD cleanly (no ghost text in letterbox bars or over
  the game).
- No measurable FPS or underrun regression with the HUD on vs. off.

## Cross-references

- Deferred enhancements #8: `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md`
- Serial-logging gotcha (why diagnostics are invisible on HDMI today): project memory `serial-logging-gotcha`
- TextCanvas / framebuffer UI pattern: `src/ui/text_canvas.{h,cpp}`
- Spec B (follow-on): in-game action hotkeys + transient toasts (to be written).
