# Hardware Checklist — UI Scaling by Output Resolution (checklist Y)

Scale rule: `max(1, min(height / 480, width / 640))` — 480p/720p 1x, 1080p 2x.
Switch modes in Settings > Video Mode (confirm-or-revert).

- [ ] Y1. 480p and 720p: every screen (ROM browser, pause menu, Settings and
      all sub-screens, HUD, toasts, boot splash) looks exactly as before.
- [ ] Y2. 1080p: ROM browser at 2x (about 13 rows) and scrolls correctly
      through a long list.
- [ ] Y3. 1080p: pause menu at 2x with the dimmed game behind it and scanlines;
      moving the selector does not lighten the background. At 1080p the
      scanlines are 2-px bands every 6 rows (coarser than the 1-px/3-row
      pattern at 1x) — expected, not a regression: same physical size and
      pixel count, just drawn at 2x.
- [ ] Y4. 1080p: Settings and every sub-screen (Controls, Hotkeys, Video Mode,
      Calibrate, Test Controllers) readable and fully on screen; Controller
      Test shows all four panels and the footer.
- [ ] Y5. Live switch 720p -> 1080p in Video Mode rescales immediately; the
      confirm-or-revert dialog is readable at the new size; reverting returns
      to 1x.
- [ ] Y6. 1080p in game: HUD and toasts the same size as before this change;
      FPS with the HUD on unchanged, no new audio underruns. The HUD/toast
      see-through checkerboard becomes 2x2 cells at 1080p (was 1x1 pixels) —
      expected, not a regression: same physical size and pixel count, just
      drawn at 2x.
- [ ] Y7. Native mode on a 1080p TV behaves like 1080p (2x). Non-16:9 native
      monitors also scale, e.g. 1280x1024 and 1680x1050 now draw menus and
      HUD at 2x (optional to test if such a monitor is available).
- [ ] Y8. Typographic boot splash is centred and readable at 1080p.
