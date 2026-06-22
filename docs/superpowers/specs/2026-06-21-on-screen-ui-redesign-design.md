# On-Screen UI Redesign — Design Spec

- **Date:** 2026-06-21
- **Status:** Approved (design); pending implementation plan
- **Source design:** Claude Design handoff `docs/Bare-metal Sega Genesis-v1.0.0-handoff.zip`
  (`project/Bare-Metal Genesis UI.dc.html`, UI spec v1.0.0)

## Goal

Adopt the Claude Design "Bare·Metal Genesis" UI spec as the visual language for the
emulator's on-screen interface, rendered natively on the 1080p framebuffer. Match the
mock's layout, color semantics, copy, and pixel-font typography — **excluding** effects
that are expensive or low-payoff on a Pi 2 (gradients, blurred glow).

The handoff is a 9-screen HTML/CSS prototype. The README is explicit: recreate the
visual output in whatever tech fits the target; do not copy the prototype's structure.
The 9 screens map almost 1:1 onto existing subsystems:

| # | Design screen | Existing code |
|---|---|---|
| 00 | Splash / title | `video/splash_draw.cpp` |
| 01 | Boot diagnostics | (new screen) |
| 02 | ROM browser | `menu/rom_menu.cpp` |
| 03 | ROM browser empty state | `rom_menu.cpp` ("No ROMs found") |
| 04 | In-game pause menu | `menu/pause_menu.cpp` |
| 05 | Settings | `menu/settings_screen.cpp` |
| 06 | Video-mode confirm/revert | `menu/video_mode_screen.cpp` |
| 07 | Controls remapping | `menu/controls_screen.cpp` |
| 08 | Save-state slot picker | `menu/save_state.cpp` |

The device-bezel / grid / changelog framing in the mock is spec-sheet presentation
chrome, not UI to build. Only each screen's inner content is in scope.

## Decisions (from brainstorming)

- **Fidelity:** Tier 1 + 2 + 3 — palette + layout, alpha blend + scanlines, and custom
  pixel fonts. Stop short of gradients/blurred glow.
- **Rollout:** pilot one screen end-to-end, verify on hardware, then replicate.
- **Pilot screen:** ROM browser (02/03).
- **Font strategy:** Approach A — two generated fonts + integer scaling.

## Current rendering constraint

All on-screen UI today goes through `src/ui/text_canvas.{h,cpp}`, which exposes exactly
two primitives over the `Display` RGB565 framebuffer:

- `FillRect` — solid RGB565 rectangle (no alpha)
- `DrawText` — one fixed monospace bitmap font (`Font12x22` via Circle's
  `CCharGenerator`), one fg + one bg color per cell

This cannot express the mock's gradients, box-shadow/glow, rounded corners, alpha,
two proportional pixel fonts, scanline blend, or CSS animation. The design is therefore
adopted as a visual-direction spec, scaled to the renderer's capabilities, with targeted
renderer extensions (below).

## Architecture

### 1. Font asset pipeline

Host-side `tools/mkfont.py` converts TTFs into Circle-format `TFont` C source that the
firmware compiles in (same struct shape `Font12x22` uses, so `CCharGenerator` semantics
are preserved). 1 bit/pixel glyphs for printable ASCII `0x20`–`0x7E`.

- **Inputs (under `tools/fonts/`, with OFL license text):**
  - Press Start 2P — rendered at its native **8px** base only.
  - VT323 — rendered at **2 fixed sizes** (~16px and ~22px body).
- **Outputs (checked in):** `src/ui/fonts/font_ps2p8.{cpp,h}`,
  `font_vt323_16.{cpp,h}`, `font_vt323_22.{cpp,h}`.
- **Build:** regeneration is a deliberate `make fonts` step, never part of the kernel
  cross-compile; the Pi build does no runtime font parsing.
- Press Start 2P title sizes (×2–×5 → 16/24/32/40px) come from **integer upscaling at
  draw time**, not extra baked files.

### 2. Renderer — `GlyphCanvas`

New `src/ui/glyph_canvas.{h,cpp}` generalizing `TextCanvas`. `TextCanvas` remains until
screens migrate, then is removed. All ops clip to the framebuffer; `Display`
integration (buffer/pitch/page) is unchanged — draws into the same front page menus
already target. Primitives:

- `FillRect(x,y,w,h,color)` — solid RGB565 (unchanged behavior).
- `BlendRect(x,y,w,h,color,alpha)` — **Tier 2.** Per-pixel alpha blend in RGB565,
  integer math (no float). For the dimmed game behind the pause panel and translucent
  highlight bars.
- `DrawText(font, scale, x, y, str, fg, bgOrTransparent)` — selects a baked font,
  integer `scale` (≥1), with a **transparent-background** mode so glyphs sit over
  panels/game without an opaque cell box. Replaces the grid-locked single-font draw.
- `DrawScanlines(x,y,w,h,strength)` — **Tier 2.** Darkens every 3rd row over a region
  (the cheap CRT cue); region-scoped to stay affordable on Pi 2.

Out of scope: gradients, blur. No `Display`/`Blit`/`Present` changes.

### 3. Theme module

`src/ui/theme.h` — palette as RGB565 constants plus semantic roles, so no screen
hardcodes raw hex (today `rom_menu.cpp` uses loose values like `0x07FF`).

- **Palette (raw → RGB565):** bg `#0a0b10`, accent/selection `#E11B22`,
  value/info `#54CBE6`, adjust `#F5B824`, active `#45C26A`, body `#ECECE4`,
  muted `#8a93a0`, dim `#69737f`.
- **Semantic roles:** `SELECTION`, `VALUE`, `ADJUST`, `ACTIVE`, `TEXT`, `TEXT_MUTED`,
  `TEXT_DIM`, `BG` — the mock's own legend made literal.
- Accent is a single constant (the mock's "tweakable accent" stays a one-line change;
  no runtime picker is built — YAGNI).

## Pilot: ROM browser (02/03)

Re-skin `menu/rom_menu.cpp` to the mock using `GlyphCanvas` + `theme.h`. Render-layer
change only — control mapping (Up/Down/Start) and FatFS data are untouched.

- **Header bar:** `ROM BROWSER` in Press Start 2P; right-aligned `SD:/roms` (cyan) +
  green `P1` dot; 2px bottom rule.
- **List:** VT323 body rows. Folders `[ Name ]` in cyan; ROM files in body color with a
  dim extension suffix. Selected row = full-width accent fill + `▶` marker (replacing
  today's cyan bar). Right-edge scrollbar track + accent thumb sized to list position.
- **Footer hint bar:** Press Start 2P micro-labels `✛ NAVIGATE`, `Ⓐ LAUNCH` (left);
  `N ROMS · M FOLDERS` count (right).
- **Empty state (03):** centered cartridge glyph (from `FillRect` blocks),
  `NO ROMS FOUND`, the `.md / .bin / .gen` hint, pulsing `WAITING FOR MEDIA` footer.
- **Scanlines:** `DrawScanlines` over the whole screen region, drawn last.
- **Icon glyphs:** `▶ ✛ Ⓐ` etc. come from small `FillRect` icon helpers — the baked
  ASCII fonts do not contain them.

## Testing

- **Host tests** (existing `test/` pure-function pattern):
  - `mkfont.py` output shape.
  - RGB565 conversion + `BlendRect` blend math.
  - `GlyphCanvas` blit into an in-memory buffer, asserting pixels.
  - Layout helpers (row Y, scrollbar thumb size/position) as unit tests, like the
    existing `hud_format` / `splash_parse` tests.
- **Hardware verify:** flash and confirm ROM browser + empty state on the TV; add a
  checklist entry matching the project's verify cadence. Renderer primitives can't be
  fully judged off-hardware.

## Rollout (after pilot passes hardware verify)

Replicate the pattern to the other 8 screens, each reusing `GlyphCanvas` + `theme.h`.
Boot-diagnostics (01) is the one genuinely new screen. Then retire `TextCanvas`.

Before building each screen, reconcile mock content against the real subsystem — e.g.
the settings mock shows `Region` and `Auto-Launch ROM` rows; confirm against
`settings.cpp` rather than treating the mock as ground truth.

## Out of scope (explicit)

- Gradients and blurred/neon glow.
- Runtime accent-color picker and scanline/glow toggles.
- Animated easing beyond simple redraw-based blink/pulse.
- The spec-sheet bezel/grid/changelog chrome.
- Multitap / 4+ player UI (already deferred project-wide).
