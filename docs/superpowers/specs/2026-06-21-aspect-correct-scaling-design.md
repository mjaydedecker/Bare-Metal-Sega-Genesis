# Aspect-Correct Video Scaling — Design

**Status:** approved design — ready for implementation plan.
**Date:** 2026-06-21
**Closes deferred video enhancements #2 (authentic-fill) and #3 (PAR correctness)**
from `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md`.

## Problem

Today `Display` offers two scaling modes via the `video_scale` setting, and
neither is both geometrically correct *and* sharp:

- **Integer** (`blit_rgb565`): uniform integer scale, square pixels, centered.
  Sharp, but geometrically wrong — a 320×224 frame shows at 1:1 square pixels, so
  content looks slightly narrow/tall versus a real console (which fills a 4:3
  display). H32 (256-wide) games pillarbox narrower than H40 (320-wide) games.
- **Stretch** (`blit_rgb565_scaled`): fills the largest 4:3 rectangle —
  geometrically correct aspect, but non-integer in *both* axes (soft/shimmery)
  and the slowest path at 1080p.

The Genesis displays both H32 (256px) and H40 (320px) active content on the same
physical 4:3 screen, so its pixels are non-square and the two modes should fill
the **same** display width. We want a mode that reproduces this correctly while
staying as sharp as possible.

## Goal

Add a third scaling mode, **Aspect**, that displays content at true 4:3
proportions (correcting PAR and equalizing H32/H40 width) while keeping vertical
resolution crisp. Expose it as a new `video_scale=aspect` option; keep `integer`
and `stretch` unchanged, and keep `integer` as the default.

Non-goals: interpolated/blurred horizontal filtering; per-region (NTSC vs PAL)
PAR distinctions; changing the default mode; scanline/CRT shader effects.

## Approach

The hybrid is **vertical integer scale + horizontal stretch to 4:3**. Crucially,
the existing `blit_rgb565_scaled` already does nearest-neighbor sampling on both
axes. If the output height is made an *exact integer multiple* of the source
height, its vertical sampling (`sy = dy * h / out_h`) collapses to clean
row-replication — crisp scanlines, no shimmer. So **no new blit primitive is
needed**: the feature is a new rect computation feeding the existing scaler.

### Geometry rule

Given framebuffer `fbW × fbH` and source frame `w × h`:

1. Largest 4:3 rectangle that fits the framebuffer:
   `rw = fbW; rh = rw*3/4;` if `rh > fbH` then `rh = fbH; rw = rh*4/3;`
   (the same 4:3 bound the stretch path already uses).
2. **Vertical = integer:** `Sv = rh / h` (floor); if `Sv < 1` then `Sv = 1`.
   `out_h = h * Sv`. Guarantees clean row-replication and a small symmetric
   letterbox.
3. **Horizontal = stretch to 4:3:** `out_w = out_h * 4 / 3`, clamped to `fbW`.
   The source width (256 **or** 320) is nearest-neighbor scaled to this same
   `out_w`, so H32 and H40 games fill the identical width (authentic-fill #2) and
   both display at correct 4:3 proportions (PAR #3).
4. **Center:** `out_x = (fbW - out_w)/2; out_y = (fbH - out_h)/2`.

Deliberate decisions:

- **Line count is ignored for the target box.** A 224-line and a 240-line frame
  both target a 4:3 box (their vertical scale differs, but the displayed box
  stays 4:3). This matches how a real console fills the screen and keeps the rule
  simple.
- **Horizontal is nearest-neighbor (no interpolation).** "Sharp" means no
  smoothing/blur; the cost is that some source columns occupy 3 destination
  pixels and some 4 (uneven column widths), matching the existing stretch path's
  character. This is the retro-preferred trade-off over blur.

### Architecture / where it plugs in

- A pure, host-testable helper computes the rect from `fbW/fbH` and `w/h`:

  ```
  // in src/video/blit.{h,cpp}
  void aspect_rect(unsigned fb_w, unsigned fb_h, unsigned w, unsigned h,
                   unsigned *out_x, unsigned *out_y,
                   unsigned *out_w, unsigned *out_h);
  ```

  No Circle dependency, unit-tested like the other blit functions.
- `Display::Blit()` gains a third branch: when `m_ScaleMode == ScaleMode::Aspect`
  and `w,h != 0`, call `aspect_rect(...)` then `blit_rgb565_scaled(...)` with that
  rect. The double-buffer / vsync `flip` + `Present()` handling is identical to
  the other branches.

## Settings wiring

Exact touch points:

- **`src/settings/settings.h`** — extend the shared enum:
  `enum class ScaleMode { Integer, Stretch, Aspect };` (already shared with
  `Display` via the include — no duplication).
- **`src/settings/settings.cpp`**
  - parse: `video_scale=aspect → ScaleMode::Aspect` (the current
    `stretch ? : integer` ternary becomes a small three-way; unknown values still
    fall back to `integer`).
  - serialize: `Aspect → "aspect"`.
- **`src/menu/settings_screen.cpp`**
  - the Integer↔Stretch toggle becomes a 3-way cycle
    Integer→Stretch→Aspect→Integer.
  - add the `< Aspect >` display label.
- **`src/video/display.cpp`** — the new `Aspect` branch described above.
- **No change** to `Display::SetScaleMode`, the kernel boot-apply, or the default
  (`Integer` stays the constructor/fallback default; `aspect` is opt-in).

## Testing

- **Host — `aspect_rect()` geometry** (`test/test_blit.cpp` or new
  `test/test_aspect.cpp`):
  - 320×224 and 256×224 into 1920×1080 both yield the **same `out_w`** and a 4:3
    box (`out_w*3 == out_h*4`, within integer rounding).
  - `out_h` is an exact integer multiple of source `h`.
  - rect is centered and within the framebuffer.
  - degenerate inputs (0×0 source, source larger than FB, tiny FB) are safe.
- **Host — settings round-trip** (`test/test_settings.cpp`): `video_scale=aspect`
  parses to `ScaleMode::Aspect` and re-serializes to `aspect`; the 3-way cycle
  helper (if extracted) covers all three values.

## Performance

Aspect writes `out_w * out_h` px/frame. At 1080p that is ~1.07M px — *fewer* than
full-stretch's ~1.55M (the vertical letterbox shrinks it), so Aspect should be no
slower than Stretch. The existing "cap stretch to 720p if it needs to be smooth"
guidance applies equally to Aspect. Vertical row-replication is cheap; the
per-pixel cost is the horizontal nearest-neighbor write, same as stretch.

## Hardware verification (add to checklist)

- `video_scale=aspect` selectable in Settings, applies live, persists across
  reboot.
- An H40/320-wide game (e.g. Sonic) and an H32/256-wide game fill the **same**
  display width and look correctly proportioned (not thin/tall).
- Vertical edges are crisp (integer); no excessive softness.
- Frame rate at the chosen `video_mode` is acceptable (compare against stretch).

## Cross-references

- Deferred enhancements list: `docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md` (#2, #3)
- Video settings design: `docs/superpowers/specs/2026-06-20-video-settings-design.md`
- FSD §4.4 (Video Output), §4.9 (Configuration)
