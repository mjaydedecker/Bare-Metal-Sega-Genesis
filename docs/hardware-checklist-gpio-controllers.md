# Hardware Checklist — GPIO Sega Controllers (checklist W)

Bench setup: DB9 jacks wired to GPIO per `src/input/sega_board.h` (jumper harness
or HAT). **DB9 pin 5 = +3.3V (NOT 5V)**, pin 8 = GND. Verify pin 5 with a
multimeter before connecting a pad.

- [ ] W1. Boot with no GPIO pad connected: USB pads still work; no phantom input.
- [ ] W2. 3-button pad on Port 1: d-pad + A/B/C/Start all register; detection
      toast shows "GPIO P1: 3-button".
- [ ] W3. 6-button pad on Port 1: A/B/C + X/Y/Z + Mode + Start all register;
      toast shows "6-button"; core runs in 6-button mode.
- [ ] W4. Port 2 pad: independent of Port 1, both players controllable.
- [ ] W5. Coexist: a USB pad on P1 and a DB9 pad on P2 both drive their players;
      either opens the pause menu (MergedMenuButtons).
- [ ] W6. In-game hotkeys (Select+button) work from the DB9 pad on P1.
- [ ] W7. Latency/perf: 60 fps maintained, no audio underruns vs USB-only.
- [ ] W8. Hot-swap: unplug/replug a DB9 pad — presence re-detects within a frame
      (polled each Poll, no hotplug event needed).
