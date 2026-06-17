# M5 Hardware Setup & Acceptance

## Card files (FAT root)
- `kernel7.img` (the M5 build)
- Firmware: `bootcode.bin`, `start.elf`, `fixup.dat`
- `GAME.MD` — a test Genesis ROM
- `config.txt` — optional (see below)

## HDMI mode — no config.txt needed
The kernel takes the firmware's **current display mode** (the same TV-supported
mode the M4 console used) and CPU integer-scales the Genesis frame into it,
centered, with black bars. So no `config.txt` HDMI settings are required — if
the card already has a working `config.txt`, leave it as-is.

> Note: an earlier draft asked for a forced 4:3 mode (`hdmi_group`/`hdmi_mode`).
> That was based on a wrong assumption (that a tiny 320x240 framebuffer would be
> GPU-scaled to the panel). On the Pi the framebuffer's physical size *is* the
> HDMI output mode, so a 320x240 request produced an "unsupported signal". Do
> **not** force a 320x240 mode.

The displayed image uses the Genesis native pixel aspect (unstretched), centered
with black bars. Exact 4:3 / pixel-aspect correction is a later enhancement.

## Serial logging
Logging now goes to the serial UART at 115200 (GPIO14/15). Attach a USB-TTL
adapter to see boot diagnostics; the HDMI output is dedicated to video.

## Acceptance checklist
- [ ] A known ROM boots to its title/first frame, visible and centered.
- [ ] Runs at correct speed (time a known-duration intro).
- [ ] An H32 (256-wide) game and an H40 (320-wide) game both display correctly.
- [ ] A game that switches resolution mid-run does not corrupt or crash.
- [ ] Serial log shows the expected boot sequence with no panics.
