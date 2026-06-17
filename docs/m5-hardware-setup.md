# M5 Hardware Setup & Acceptance

## Card files (FAT root)
- `kernel7.img` (the M5 build)
- Firmware: `bootcode.bin`, `start.elf`, `fixup.dat`
- `GAME.MD` — a test Genesis ROM
- `config.txt` — see below

## config.txt — force a 4:3 HDMI mode
The kernel renders a fixed 320x240 (4:3) surface; the firmware scales it to the
HDMI signal. Force a 4:3 signal so the TV pillarboxes it (tune to your panel):

    hdmi_group=2
    hdmi_mode=16     # 1024x768 (4:3). Alt: hdmi_mode=35 (1280x960).

Set one mode, verify on the TV, adjust if the panel rejects it. No code depends
on the exact mode.

## Serial logging
Logging now goes to the serial UART at 115200 (GPIO14/15). Attach a USB-TTL
adapter to see boot diagnostics; the HDMI output is dedicated to video.

## Acceptance checklist
- [ ] A known ROM boots to its title/first frame, visible and centered.
- [ ] Runs at correct speed (time a known-duration intro).
- [ ] An H32 (256-wide) game and an H40 (320-wide) game both display correctly.
- [ ] A game that switches resolution mid-run does not corrupt or crash.
- [ ] Serial log shows the expected boot sequence with no panics.
