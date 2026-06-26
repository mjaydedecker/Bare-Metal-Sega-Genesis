# Bare Metal Sega Genesis

A bare-metal Sega Genesis / Mega Drive emulator for the Raspberry Pi. It boots
straight to the hardware with **no Linux, no operating system** — the emulator
*is* the kernel. Inspired by [BMC64](https://github.com/randyrossi/bmc64) (a
bare-metal Commodore 64), it combines the [Circle](https://github.com/rsta2/circle)
bare-metal C++ environment with the [Genesis-Plus-GX-Wide](https://github.com/libretro/Genesis-Plus-GX-Wide)
libretro core to deliver instant-on, low-latency emulation.

> **Status:** Targets the **Raspberry Pi 2 (32-bit / ARMv7)**, producing
> `kernel7.img`. Achieves a clean 60 fps with full audio on real hardware.

## Why bare metal?

- **Instant on** — no OS to boot; you're at the ROM browser in a second or two.
- **Low latency** — direct control of video, audio, and USB with nothing in the way.
- **Deterministic** — one program owns the whole machine.

## Features

- **Emulation** — Genesis / Mega Drive via the Genesis-Plus-GX-Wide libretro core, with 3- and 6-button controller support.
- **On-screen ROM browser** — pick games from the SD card, including subfolders, with a custom pixel-font UI.
- **In-game pause menu** — save/load states (4 slots per game), settings, and controls without leaving the game.
- **Save states & battery saves** — instant save states plus automatic SRAM (`.srm`) persistence to SD.
- **Video options** — integer, aspect-correct (4:3), or stretch scaling; selectable HDMI output mode; widescreen; optional tear-free vsync page-flipping.
- **Audio options** — HDMI, 3.5 mm analog (PWM), or I2S DAC output; master volume, mute, and configurable latency.
- **Two controllers** — Player 1 + 2 USB gamepads with hotplug, per-port remapping, and per-device auto-calibration.
- **Real Sega controllers via GPIO** *(experimental)* — original 3- and 6-button DB9 Genesis/Mega Drive pads wired directly to the GPIO header, on both ports, coexisting with USB. Live 3/6-button auto-detection. Decode/sequencing logic is complete and host-tested, but **not yet verified on hardware** (see [Real Sega controllers](#real-sega-controllers-experimental)).
- **In-game hotkeys & toasts** — quick save/load, volume, HUD toggle, and mute via remappable Select+button combos.
- **Diagnostics HUD** — optional FPS / underrun / queue / ROM+mode overlay.
- **Boot splash** — embedded logo shown during init, overridable from SD.

## Hardware requirements

- **Raspberry Pi 2** (Model B, ARMv7 / `kernel7.img`).
- A **microSD card** (FAT32) for the kernel, ROMs, and saves.
- HDMI display; USB gamepad(s).
- Optional: 3.5 mm analog audio or a PCM5102 I2S DAC.
- Optional *(experimental)*: original Sega DB9 controllers wired to the GPIO
  header — see [Real Sega controllers](#real-sega-controllers-experimental).

## Building

The build cross-compiles on a Linux host using the `arm-linux-gnueabihf`
toolchain (available via `apt` on Debian/Ubuntu).

### Prerequisites

```sh
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf make
```

### Clone with submodules

Circle and the Genesis-Plus-GX-Wide core are git submodules:

```sh
git clone --recurse-submodules https://github.com/mjaydedecker/Bare-Metal-Sega-Genesis.git
cd Bare-Metal-Sega-Genesis
# or, if you already cloned:
git submodule update --init --recursive
```

### Compile

```sh
make
```

This builds the Circle sub-libraries, the Genesis core (`libgenesis.a`), and
links everything into **`kernel7.img`**. The Makefile is already configured for
`RASPPI=2`, `AARCH=32`, and the `arm-linux-gnueabihf-` prefix.

## SD card setup

1. Format a microSD card as **FAT32**.
2. Copy the Raspberry Pi firmware files (`bootcode.bin`, `start.elf`,
   `fixup.dat`) and a `config.txt` to the card root. A ready-to-use sample for
   the Pi 2 is provided at [`boot/config.txt`](boot/config.txt); see the Circle
   [boot documentation](https://github.com/rsta2/circle/tree/master/boot) for
   details and where to obtain the firmware files.
3. Copy the built **`kernel7.img`** to the card root.
4. Create a **`roms/`** folder and place your `.bin` / `.md` / `.gen` Genesis
   ROM files inside (subfolders are supported). *ROMs are not included — supply
   your own legally-obtained dumps.*
5. (Optional) `settings.txt`, `controllers.txt`, and `splash.raw` are written
   to / read from the card at runtime to persist configuration and override the
   boot logo.

Insert the card, connect HDMI and a USB controller, and power on.

## Controls

- Navigate menus with the D-pad; **confirm with START**.
- In game, press **Start + Select** to open the pause menu.
- Hold **Select + a button** for in-game hotkeys (quick save/load, volume, HUD,
  mute) — all remappable from the menus.

## Real Sega controllers (experimental)

Original Sega Genesis / Mega Drive DB9 controllers can be read directly off the
Raspberry Pi GPIO header — no USB adapter — alongside the USB gamepads. Both
ports and live 3-/6-button auto-detection are supported.

> **Status:** firmware-complete and host-tested, but **not yet verified on real
> hardware**. There is no finished adapter board yet; the physical HAT is a
> separate project. Treat this as wiring-at-your-own-risk until checklist
> [`docs/hardware-checklist-gpio-controllers.md`](docs/hardware-checklist-gpio-controllers.md)
> has been run on a Pi 2.

**Electrical contract (important):** a Genesis pad is an *active* device whose
internal multiplexer drives the data lines to its supply rail. **Power the pad
from 3.3 V (DB9 pin 5 → the Pi's 3.3 V rail), not 5 V.** At 3.3 V the data lines
swing 0–3.3 V, which is safe for the Pi's GPIO, so the DB9 lines can be wired
**directly with no level shifters, dividers, or shift registers**. Powering at
5 V drives ~5 V into the non-5 V-tolerant GPIO and can damage the SoC.

The exact GPIO pin assignment (per port: SELECT + six data lines) lives in
[`src/input/sega_board.h`](src/input/sega_board.h) — the single source of truth
the adapter is built to. Pads are polled once per frame, and a "GPIO P1/P2:
3-/6-button" toast confirms detection on game load.

## Project layout

```
src/
  kernel.cpp        top-level orchestrator (boot, main loop)
  libretro/         glue to the Genesis-Plus-GX-Wide core (environment, callbacks)
  video/            framebuffer display, scaling/blit, boot splash
  audio/            audio driver (HDMI / PWM / I2S) and helpers
  input/            USB + GPIO (real Sega DB9) gamepad handling, mapping, hotkeys, calibration
  storage/          SD card / FatFS access
  menu/             ROM browser, pause menu, save states, SRAM, settings screens
  settings/         persisted settings model + store
  ui/               pixel-font renderer, glyph canvas, HUD, overlay, fonts
tools/              mkfont.py / mksplash.py asset converters
test/               host-side unit tests (run with the system compiler)
libs/               circle/ and genesis-plus-gx-wide/ submodules
docs/               implementation plans, specs, hardware setup notes
```

## Tests

Pure logic (blit, scaling, menu windowing, settings parsing, input mapping,
fonts, etc.) is covered by host-side unit tests that build with the native
compiler:

```sh
cd test
make run
```

## Acknowledgements

- [Circle](https://github.com/rsta2/circle) by Rene Stange — the bare-metal Raspberry Pi C++ environment.
- [Genesis-Plus-GX-Wide](https://github.com/libretro/Genesis-Plus-GX-Wide) — the libretro Genesis emulation core.
- [BMC64](https://github.com/randyrossi/bmc64) — the bare-metal-emulator inspiration.
- Fonts: [Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) and [VT323](https://fonts.google.com/specimen/VT323) (SIL Open Font License).

## License

The emulator code, Circle, and Genesis-Plus-GX-Wide are covered by their
respective licenses (see each submodule). Genesis-Plus-GX is distributed under
a non-commercial license — review it before redistribution. No game ROMs are
included with this project.
