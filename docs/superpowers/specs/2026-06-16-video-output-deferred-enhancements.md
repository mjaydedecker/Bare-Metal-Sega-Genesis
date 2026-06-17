# Video Output — Deferred / Higher-Order Enhancements

**Status:** backlog — revisit after the M5 base concept is proven on hardware.

## Context

The M5 base design deliberately picks the simplest path that gets the Genesis
picture on screen at full speed on a **Raspberry Pi 2**:

- GPU-scaled output (firmware scales a small framebuffer up to the panel).
- 16bpp framebuffer; the core's RGB565 frames are blitted directly with **no
  per-pixel conversion**.
- A fixed **320×240** (4:3) surface; the active frame is centered inside it and
  the GPU scales the whole surface to a 4:3-configured HDMI output.
- Minimal scope: no configurable toggles, no widescreen.

During brainstorming we considered several higher-order alternatives. They were
deferred to keep the first milestone small and de-risked. This document records
them so we can pick them up later. Each notes **what it adds**, **why it was
deferred**, and **where it would plug in**.

---

## 1. CPU integer-scaling path (sharper, board-aware)

**What it adds:** Instead of letting the GPU scale a small framebuffer, allocate
a large framebuffer at the panel resolution (720p/1080p) and do nearest-neighbor
**integer** scaling + centering on the CPU. Gives crisp, evenly-sized pixels and
full control over scale factor and letterboxing, independent of the GPU scaler's
filtering. This is the original plan's M5 approach (Impl-Plan §M5, FSD §4.4.2).

**Why deferred:** Heavy on a Pi 2 — ~480 MB/s of writes at 1080p/60 with format
conversion. Shines on a Pi 4.

**Where it plugs in:** `Display` gains an alternate blit path / strategy. Could be
selected by board (`RASPPI`) or a config setting, sharing the same `Blit()`
interface. A 16bpp middle-ground (integer scale, but keep the framebuffer 16bpp
so RGB565 still copies without conversion) is a cheaper variant worth measuring
first.

## 2. Authentic-fill via native-resolution framebuffer (Approach B)

**What it adds:** Size the framebuffer to the current native resolution and let
the GPU stretch each mode to fill the 4:3 display. An H32 (256-wide) game then
fills the full screen width like a real console, instead of sitting pillarboxed
inside the fixed 320-wide surface (the cosmetic compromise in the M5 base).

**Why deferred:** Re-allocating the framebuffer via the mailbox on a mid-game
H32↔H40 (or 224↔240) switch can cause a brief flicker and adds moving parts.

**Where it plugs in:** `Display::Blit()` detects a geometry change and re-creates
`CBcmFrameBuffer` at the new native size instead of re-centering within a fixed
surface.

## 3. Pixel aspect ratio (PAR) correctness

**What it adds:** Genesis pixels are not square. Strictly correct 4:3 output maps
H32 (256px) and H40 (320px) content to the **same** displayed width with
PAR-aware horizontal scaling. The M5 base ignores PAR and just places native
pixels into the 320×240 surface.

**Why deferred:** Needs real (often non-integer) horizontal scaling, which pairs
naturally with the CPU-scaling path (#1) or Approach B (#2).

**Where it plugs in:** horizontal scale factor in the chosen scaling strategy.

## 4. Configurable scaling modes — integer vs. stretch *(already in FSD)*

Integer (nearest-neighbor, sharp) vs. full-screen aspect-corrected stretch,
selectable at runtime. Already specified: **FSD §4.4.2** and the `video_scale`
setting in **FSD §4.9 config table**. Deferred from M5 minimal; implement
alongside #1.

## 5. Widescreen mode *(already in FSD)*

Genesis-Plus-GX-Wide's extended horizontal resolution (wider than 320). Needs a
wider surface and core-side configuration. Already specified: **FSD §4.4.3** and
**§4.9**. Deferred from M5 minimal.

## 6. Tear-free output / vsync / double buffering *(partly in FSD)*

**What it adds:** Eliminate tearing by page-flipping between two buffers synced to
the display. The M5 base is single-buffered and accepts possible tearing.

**Why deferred:** Adds buffer management and timing; not needed to prove the base
concept. Already noted as display-capability-dependent in **FSD §4.4.4**.

**Where it plugs in:** allocate `CBcmFrameBuffer` with virtual height = 2×
physical and use `SetVirtualOffset()` to flip; blit to the off-screen page.

## 7. config.txt-driven video settings

**What it adds:** Drive region, scale mode, and auto-launch from `config.txt`
(**FSD §4.9** config table), and finalize robust 4:3 HDMI handling across panels
(rather than the single documented 4:3 mode the M5 base assumes).

**Why deferred:** Needs the config parser (a later milestone) and per-panel
testing.

## 8. On-screen logging / debug overlay retention

**What it adds:** Keep a debug overlay (or restore the text console) on top of
video. In the M5 base, the framebuffer is handed entirely to video and logging
moves to the serial UART.

**Why deferred:** Overlay compositing is extra work; serial logging is sufficient
for bring-up.

---

## Cross-references

- FSD §4.4 (Video Output), §4.9 (Configuration) — `Documents/Bare-Metal-Sega-Genesis-FSD.md`
- Implementation Plan §M5 — `Documents/Bare-Metal-Sega-Genesis-Implementation-Plan-Phase1.md`
- M5 base design — `docs/superpowers/specs/2026-06-16-m5-video-output-design.md` *(to be written)*
