# Changelog

All notable changes to Bare Metal Sega Genesis are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the
project uses [Semantic Versioning](https://semver.org/). The version lives in
`src/version.h`; each release commit is tagged `v<version>`.

## [0.1.0] - 2026-09-11

First release. This is a pre-1.0 version: it is usable, but expect changes,
and some features have not yet been verified on real hardware (listed below).

### Supported boards
- Raspberry Pi 2 (`kernel7.img`, the default build), Raspberry Pi 3
  (`kernel8-32.img`) and Raspberry Pi 4 (`kernel7l.img`), all in 32-bit
  mode. All three have been verified on real hardware. Pi 5 is not supported.
- `make dist` builds a ready-to-copy SD-card zip for each board (kernel,
  Raspberry Pi firmware, `config.txt`, empty `roms/` folder, licences).

### Added
- Sega Genesis / Mega Drive emulation using the Genesis-Plus-GX-Wide libretro
  core, booting straight to the hardware with no operating system. Runs at a
  clean 60 fps with full audio on a Raspberry Pi 2.
- On-screen ROM browser for `SD:/roms`, including subfolders.
- In-game pause menu (Start+Select by default): save and load states (4 slots
  per game), reset, settings, and return to the ROM browser.
- Automatic battery-save (SRAM) persistence to the SD card.
- Video settings: integer, aspect-correct (4:3) or stretch scaling; HDMI
  output mode picker with confirm-or-revert; widescreen; optional tear-free
  vsync page flipping.
- On-screen menus, boot splash and HUD scale with the output resolution
  (2× at 1080p).
- Audio settings: HDMI, 3.5 mm analog or I2S DAC output; volume, mute and
  latency.
- Two USB controllers with hotplug, per-player button remapping, per-device
  calibration, and 3- or 6-button pad type.
- Remappable in-game hotkeys (quick save/load, volume, HUD, mute) with
  on-screen notifications, and an optional diagnostics HUD.
- Settings > Test Controllers: live button test for USB pads and both ports of
  the Sega controller HAT.
- Boot splash, with an optional image override from `SD:/splash.raw`.
- Version shown on the boot splash, in the Settings header and in the boot log.

### Not yet verified on real hardware
- Original Sega DB9 controllers wired to GPIO or through the controller HAT
  (experimental), including menu navigation from HAT pads.
- 3.5 mm analog (PWM) and I2S DAC audio output; volume, mute and latency
  settings.
- Battery-save (SRAM) persistence.
- UI scaling at 1080p and above; aspect-correct scaling; HDMI mode
  auto-revert after an unsupported signal.
- In-game hotkeys and hotkey remapping; the redesigned HUD and notifications.
- Boot splash image override; scrolling menu lists at 480p.

### Licensing
- The repository has no top-level licence file yet. Genesis Plus GX may not be
  sold or used commercially, and Circle is licensed under GPLv3; review both
  before redistributing builds.

[0.1.0]: https://github.com/mjaydedecker/Bare-Metal-Sega-Genesis/releases/tag/v0.1.0
