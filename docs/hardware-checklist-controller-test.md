# Hardware Checklist — Controller Test Screen + Menu GPIO Polling (checklist X)

Bench setup: Genesis Controller HAT seated (or DB9 harness per
`src/input/sega_board.h`), at least one 3-button and one 6-button Sega pad, one
USB pad. **DB9 pin 5 = +3.3V (NOT 5V).**

Menu GPIO polling (HAT pads previously read stale/zero in every menu):

- [ ] X1. ROM browser: a HAT pad on Port 1 moves the selection and launches a
      ROM with Start (no USB pad connected).
- [ ] X2. In game, a HAT pad opens the pause menu, navigates it, and opens
      Settings; no doubled or missed presses.
- [ ] X3. Gameplay unchanged: 60 fps, no audio underruns, 6-button detection
      toast still correct on ROM load (poll gate didn't starve the frame poll).

Controller Test screen (Settings > Input > Test Controllers):

- [ ] X4. Opening the row with Start (and keeping Start held) does NOT exit;
      the hold only counts after Start is released once.
- [ ] X5. 3-button pad on HAT Port 1: panel shows `3-BUTTON`; d-pad, A, B, C,
      Start light while held; X/Y/Z/Mode shown greyed and never light.
- [ ] X6. 6-button pad on HAT Port 1: panel shows `6-BUTTON`; all 12 controls
      light, including X/Y/Z/Mode.
- [ ] X7. HAT Port 2 panel reacts only to the Port 2 pad (ports independent).
- [ ] X8. USB pads: `USB 1` / `USB 2` panels show the VID:PID and light
      buttons (C = RB, Z = LB, Mode = Select after calibration).
- [ ] X9. Hot-plug: unplug a HAT pad and a USB pad — panels switch to
      `NO PAD` within about a second; replug restores them.
- [ ] X10. Hold Start ~2 s on any pad exits back to Settings; the footer bar
      fills while held and empties on an early release; a quick tap just
      lights Start.
- [ ] X11. At 480p output mode all four panels and the footer fit on screen.
- [ ] X12. Leaving the screen with Start held doesn't trigger anything in
      Settings.
