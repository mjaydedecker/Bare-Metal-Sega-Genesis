# In-Game HUD / Overlay Redesign — Design

**Date:** 2026-06-22
**Status:** Approved (brainstorm), pending implementation plan
**Relates to:** `project_ui_redesign`, `project_overlay_hud`, `project_ingame_hotkeys_toasts`

## Summary

Re-skin the in-game diagnostics HUD and transient toast notices — the last
on-screen UI still rendered with the legacy `TextCanvas` — into the Claude
Design v1.0.0 look using the new `GlyphCanvas` renderer. The HUD also gains
semantic health coloring and a few newer emulator-state fields. All formatting
and health-classification logic moves into the pure, host-tested `hud` layer so
the renderer stays a dumb drawer.

This is the final visual-redesign task. It does **not** fully retire
`TextCanvas` (see Scope Boundary).

## Goals

- HUD and toasts match the v1.0.0 design: translucent panels, scanline texture,
  pixel fonts, theme palette.
- HUD values are color-coded by health so problems are visible at a glance.
- HUD surfaces current emulator state, including newer settings (vsync,
  widescreen, audio latency) in addition to the existing fields.
- Toasts convey success / failure / info via color.
- No regression to the tuned vsync/pacing path.

## Non-Goals

- Fully retiring `TextCanvas` (blocked on a `GlyphCanvas` image-blit primitive;
  `splash_draw.cpp` still uses `TextCanvas` for the `splash.raw` image path).
- Changing what triggers the HUD or toasts (settings/hotkeys unchanged).
- Changing the compositing model (`Display::Blit`/`Present` untouched).

## Decisions (from brainstorm)

| Question | Decision |
|----------|----------|
| HUD panel treatment | Translucent blended panel + scanlines (matches pause menu) |
| Value coloring | Semantic health colors (FPS/UR/OR by state; info fields cyan) |
| Toast treatment | Translucent pill, colored by success / fail / info |
| HUD fields | Existing set **plus** newer state: vsync, widescreen, audio latency |

## Architecture

Three layers, mirroring the existing split:

1. **Pure layer — `src/ui/hud.{h,cpp}`** (no Circle deps, host-tested)
2. **Renderer — `src/ui/overlay.{h,cpp}`** (Circle-side, draws via `GlyphCanvas`)
3. **Driver — `src/kernel.cpp`** (populates stats, triggers toasts)

### 1. Pure layer (`src/ui/hud.{h,cpp}`)

Extend `HudStats` with newer state:

```c
struct HudStats {
    unsigned    fps, underruns, overruns, queued, target;
    const char *rom, *mode, *scale;   // existing
    bool        vsync;                 // new
    bool        widescreen;            // new
    const char *latency;              // new: "low" | "medium" | "high"
};
```

Add a health enum and a cell-based builder that **replaces** the old flat-line
`hud_format()`:

```c
enum HudHealth { HUD_GOOD, HUD_WARN, HUD_BAD, HUD_INFO };

struct HudCell {
    char      label[HUD_LABEL_MAX + 1];  // e.g. "FPS"
    char      value[HUD_VALUE_MAX + 1];  // e.g. "60"
    HudHealth health;
};

// Fill up to max cells; returns count.
unsigned hud_build(const HudStats &s, HudCell cells[], unsigned max);
```

Health rules (pure, unit-tested at boundaries):

- **FPS:** `>= 58` GOOD, `>= 50` WARN, else BAD.
- **underruns / overruns:** `0` GOOD, else BAD.
- **queue (AQ q/t), ROM, mode/scale, vsync, widescreen, latency:** INFO.

Cell order (one metric per cell, so each value carries its own health color):
`FPS`, `AQ` (queued/target), `UR`, `OR`, `ROM` (dir stripped, truncated),
`MODE` (mode/scale), `VSYNC` (on/off), `WIDE` (on/off), `LAT` (latency). The
pure layer only orders the cells; the **renderer** lays them out into rows
(pairing short cells two-per-row, e.g. `FPS | AQ`, `UR | OR`; full-width cells
like `ROM` on their own row), so the panel is ~5–6 rows tall, not 9.

Number→string formatting uses the existing manual-digit approach (no `snprintf`
on bare metal — see `hud.cpp` `app_uint`).

### 2. Renderer (`src/ui/overlay.{h,cpp}`)

- Constructor retargets `TextCanvas*` → `GlyphCanvas*`. HUD font:
  `g_font_vt323_16` (6×16 body).
- `Draw(const HudStats &)`:
  1. `hud_build()` → cell array.
  2. Lay cells into rows (short cells paired two-per-row, full-width cells
     alone); size the panel from the resulting row count; top-left, small margin.
  3. `BlendRect` dark panel (`theme::BG`, alpha ~190) + `Scanlines` over it.
  4. Per cell: label in `theme::TEXT_MUTED`, value colored by a renderer-side
     `health→color` map: GOOD→`ACTIVE`, WARN→`ADJUST`, BAD→`SELECTION`,
     INFO→`VALUE`.
- Toasts:
  ```c
  enum ToastKind { TOAST_INFO, TOAST_SUCCESS, TOAST_FAIL };
  void ShowToast(const char *msg, ToastKind kind = TOAST_INFO);
  ```
  `DrawToast()` draws a translucent pill (`BlendRect`) bottom-center, text
  colored by kind (SUCCESS→`ACTIVE`, FAIL→`SELECTION`, INFO→`VALUE`/`ADJUST`),
  with the existing ~2 s (`TOAST_FRAMES`) decay.
- Both the panel and pill keep a **full fixed-size repaint each frame** so no
  stale pixels persist across the two vsync pages (same correctness property as
  today's opaque box, now blended).

### 3. Driver (`src/kernel.cpp` / `kernel.h`)

- Construct `m_Overlay(&m_GlyphCanvas)`.
- Populate new `HudStats` fields each frame from `m_Settings`:
  `st.vsync = m_Settings.vsync; st.widescreen = m_Settings.widescreen;
  st.latency = audio_latency_file_value(m_Settings.audio_latency);`
- Pass `ToastKind` at the existing ~6 `ShowToast` sites from the result already
  in hand: quick-save/load → SUCCESS/FAIL, "No quick save" → FAIL, volume / HUD
  on-off / mute → INFO.
- `m_Canvas` (`TextCanvas`) **stays** in `kernel.h` — splash still uses it.

## Compositing & Performance

- Drawn after `retro_run()` on the visible front page via `GlyphCanvas` (which
  already targets `Display`'s front page). **No changes to `Display::Blit` /
  `Present`** — the tuned vsync/pacing path (`project_tear_free_output`) is
  untouched.
- HUD is gated by `debug_overlay` (default OFF), so the larger blended panel
  only renders when explicitly enabled. Toasts are small and transient.
  `BlendRect` over a small region per frame is inexpensive. Per-frame cost is a
  hardware-verify item, not a design risk.

## Scope Boundary (TextCanvas retirement)

After this change, `TextCanvas` is still used by `splash_draw.cpp`
(`splash_show_embedded`, `splash_apply_override`) to blit the `splash.raw`
RGB565 image, because `GlyphCanvas` has no image-blit primitive. Fully retiring
`TextCanvas` is a **separate follow-up**: add an image blit to `GlyphCanvas`
(or `Display`), migrate the splash image path, then delete `TextCanvas`. Out of
scope here.

## Testing

- **Host (`test/test_hud.cpp`):** extend for `hud_build` — FPS health at
  boundaries (57/58, 49/50), UR/OR zero vs nonzero, presence/format of the new
  vsync/widescreen/latency cells, ROM truncation.
- **Circle-side:** `overlay.cpp` / `GlyphCanvas` are not host-run (Circle deps);
  verified by kernel build + hardware, consistent with existing practice.
- **Hardware checklist (new section):**
  - HUD readability over bright/busy scenes (translucent panel + scanlines).
  - Panel size/placement correct (~5–6 rows after pairing).
  - Semantic colors correct — force an underrun to confirm red; confirm FPS
    green at 60.
  - Toast colors correct — quick-save (green) vs "No quick save" (red) vs volume
    (info).
  - No FPS/pacing regression with HUD on and off.

## Risks

- **Legibility of translucent panel over bright scenes** — mitigated by panel
  alpha + scanline darkening; confirm on hardware.
- **Panel height** (~5–6 rows after pairing) could cover more of the frame than
  the old 4-line box — confirm placement is acceptable; trim/pair further if
  needed.
- **Per-frame blend cost** with HUD enabled — expected negligible; measure via
  the HUD's own FPS readout.

## Files Touched

- `src/ui/hud.h`, `src/ui/hud.cpp` — extend `HudStats`, add `HudHealth` /
  `HudCell` / `hud_build`, replace `hud_format`.
- `src/ui/overlay.h`, `src/ui/overlay.cpp` — `GlyphCanvas` retarget, health
  coloring, toast kinds, translucent panel + pill.
- `src/kernel.cpp`, `src/kernel.h` — construct with `&m_GlyphCanvas`, populate
  new fields, pass `ToastKind` at call sites.
- `test/test_hud.cpp` — cover `hud_build`.
- `docs/hardware-verification-checklist-*.md` — new HUD-redesign section.
