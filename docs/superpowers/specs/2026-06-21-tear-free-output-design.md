# Tear-Free Video Output (Double-Buffer + Vsync) — Design

**Date:** 2026-06-21
**Status:** approved — ready for implementation plan
**Phase:** 3 (Polish) — FSD §4.4.4; deferred video enhancement #6
(`docs/superpowers/specs/2026-06-16-video-output-deferred-enhancements.md`)

## Goal

Eliminate screen tearing during gameplay by presenting completed frames with a
vsync-synced page flip instead of blitting directly into the live, scanned-out
framebuffer. Exposed as a `vsync` setting (default **on**) so it can be disabled
if the vblank wait disturbs the Pi 2's pacing.

## Background

Current flow: `retro_run()` → core → `video_refresh_cb` → `Display::Blit()`,
which scales the RGB565 frame straight into the single visible framebuffer
(`src/video/display.cpp`). The display scans that buffer out concurrently, so
mid-frame writes during scrolling tear.

On-screen UI (`TextCanvas`, pause/settings menus) draws into the framebuffer via
`Display::Buffer()` as immediate-mode overlays. Per `display.h`, `Buffer()` is
UI-only; game frames always go through `Blit()`.

Pacing is the project's tuned timer + audio-queue high-watermark gate in
`kernel.cpp` — deliberately **not** vsync-driven. The vblank wait this feature
adds is the one real risk to that pacing, which is why it is a toggle.

## Non-goals (out of scope)

- Triple buffering / async flip.
- Changing the pacing model (timer + audio gate stays the rate authority; the
  vsync wait only aligns the *flip*, it must not become the frame clock).
- Any change to `blit.cpp` scaling math, the libretro callback, the kernel
  pacing loop, or menu/UI code.

## Design

All changes are contained in `Display` plus the settings plumbing for the toggle.
The libretro callback, kernel pacing loop, and every menu/UI file are untouched.

### Two-page framebuffer

`Display` allocates the framebuffer with virtual height = 2× physical
(`new CBcmFrameBuffer(w, h, FB_DEPTH, w, 2*h)`), giving two stacked pages:
page 0 at y=0, page 1 at y=`m_FbH`. It tracks:

- `m_pFront` — base pointer of the page currently scanned out (visible).
- `m_pBack` — base pointer of the off-screen page (game draw target).
- `m_FrontIsPage0` — which physical page is front (to compute the flip offset).
- `m_DoubleBuffered` — whether the 2-page allocation succeeded.
- `m_Vsync` — the live setting value.

`Buffer()` returns **`m_pFront`** — UI/menus keep drawing to the visible page
exactly as today, so no UI code changes and static overlays never tear.

### Blit + present

`Blit()` chooses its target:

- **vsync on and double-buffered:** draw the scaled frame into `m_pBack`, then
  call `Present()`.
- **otherwise (vsync off, or single-buffer fallback):** draw into `m_pFront`
  directly and do **not** flip — byte-for-byte today's behavior.

`Present()` (private):
1. `m_pFB->WaitForVerticalSync()` — block until vblank (tear-free flip).
2. `m_pFB->SetVirtualOffset(0, m_FrontIsPage0 ? m_FbH : 0)` — show the page just
   drawn (the back page).
3. Swap `m_pFront`/`m_pBack` and flip `m_FrontIsPage0`.

Because each `Blit()` fully overwrites its target page's active area, stale
content on the alternate page (including a just-closed menu) is overwritten
before it is ever shown — menu open/close stays artifact-free with no extra work.

### Letterbox repaint across both pages

`Blit()` today clears the framebuffer (letterbox/pillarbox bars) only on a
frame-size change or via `ForceRepaint()`. With two pages, a bar-area artifact
can live on the *other* page. To keep both pages clean:

- `ForceRepaint()` (called after a menu closes) clears **both** pages
  immediately (memset the full virtual framebuffer), then forces the next
  `Blit()` to redraw — so menu remnants in the bars cannot survive a flip.
- On a frame-size change, `Blit()` clears **both** pages before drawing (rare:
  H32↔H40 / 224↔240 switches), avoiding a one-frame stale-bar flash after the
  flip.

A single private helper `ClearBlack()` clears the active virtual framebuffer
(both pages when double-buffered, one when not).

### The `vsync` setting

- `Settings` gains `bool vsync;` default `true`.
- Parse key `vsync` (truthy: on/true/1/yes); serialize `vsync=on|off`.
- `Display::SetVsync(bool)` sets `m_Vsync` live (takes effect next `Blit`).
- Kernel applies `m_Display.SetVsync(m_Settings.vsync)` at startup alongside the
  existing `SetScaleMode`.
- Settings screen gains a **"Vsync:"** `< On >`/`< Off >` toggle row, applied
  live (like Video Scale).

### Graceful degradation

If the 2-page allocation fails in `Initialize()` or `SetMode()`, fall back to a
single-page framebuffer: `m_DoubleBuffered = false`, `m_pFront == m_pBack`,
`Present()` is never called, behavior == today. `Initialize()`/`SetMode()` keep
their current keep-on-fail contracts (build the new FB before discarding the old;
on failure the old mode stays intact). If `WaitForVerticalSync()` is unsupported
by firmware it is a fast no-op — no hang, just no tear-free benefit.

## Testing

- **Host unit test** (extend `test/test_settings.cpp`): `vsync` defaults to
  `true`; `vsync=off` parses false; unknown/missing → default true; round-trip
  serialize→parse preserves it.
- **Blit math** is unchanged (`blit.cpp` already host-tested); no new host test
  needed for the flip path (hardware-only).
- **Hardware-verification checklist** — new section:
  - K1: scrolling game (e.g. Sonic) shows no tearing with vsync on.
  - K2: toggle Vsync off in Settings → tearing returns (proves the toggle and
    that "off" == old path); toggle back on → gone.
  - K3: no audio/pacing regression with vsync on (no new underruns/overruns in
    the periodic metrics; game runs full speed).
  - K4: open/close the pause menu repeatedly → no menu remnants or bar
    artifacts after resuming.
  - K5: `vsync=on` persisted in `SD:/settings.txt`; survives reboot.

## Files touched

- `src/video/display.h`, `src/video/display.cpp`
- `src/settings/settings.h`, `src/settings/settings.cpp`
- `src/kernel.cpp` (one `SetVsync` call at startup)
- `src/menu/settings_screen.cpp` (Vsync row)
- `test/test_settings.cpp`
- `docs/hardware-verification-checklist-2026-06-20.md` (new section K)
