# M7 Controller Input — Acceptance

## Setup
- Flash the M7 `kernel7.img`.
- Plug a USB gamepad into the Pi **before powering on** (hotplug is not
  supported in M7).
- A test Genesis ROM as `GAME.MD` on the card.

## Acceptance checklist
- [ ] All four D-pad directions move the character/menu correctly.
- [ ] The six face buttons (Genesis A/B/C/X/Y/Z) each do something in a game
      that uses them.
- [ ] Start pauses / starts; Mode (mapped to Select) behaves as Mode.
- [ ] With NO gamepad connected, the game still boots and runs; the serial log
      shows "No USB gamepad found".

## If a button is wrong / missing
- Wrong A/B/C feel: that is the core's standard 6-button mapping; remapping is a
  later milestone.
- D-pad dead on a particular pad: that pad may report the hat/axes without the
  normalized direction bits — a known deferred limitation (raw hat/axis
  fallback).
- No buttons at all: confirm the pad enumerated (it registers as "upad1"); try a
  different USB pad; ensure it was connected at boot.
