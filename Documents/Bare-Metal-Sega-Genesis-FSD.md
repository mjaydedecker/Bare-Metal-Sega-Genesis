# Functional Specification Document
# Bare Metal Sega Genesis

**Version:** 1.0
**Date:** 2026-04-06
**Platform:** Raspberry Pi (Bare Metal)
**Language:** C / C++
**Repository:** https://github.com/mjaydedecker/Bare-Metal-Sega-Genesis

---

## Revision History

| Version | Date | Summary |
|---|---|---|
| 1.0 | 2026-04-06 | Initial release — core emulation, video/audio output, controller input, ROM loading, menu system, save states |

---

## Table of Contents

1. [Overview](#1-overview)
2. [Scope](#2-scope)
3. [System Architecture](#3-system-architecture)
4. [Functional Requirements](#4-functional-requirements)
   - 4.1 [Boot and Startup](#41-boot-and-startup)
   - 4.2 [ROM Browser](#42-rom-browser)
   - 4.3 [Emulation Core](#43-emulation-core)
   - 4.4 [Video Output](#44-video-output)
   - 4.5 [Audio Output](#45-audio-output)
   - 4.6 [Controller Input](#46-controller-input)
   - 4.7 [Save States](#47-save-states)
   - 4.8 [In-Emulation Menu](#48-in-emulation-menu)
   - 4.9 [Configuration](#49-configuration)
5. [Non-Functional Requirements](#5-non-functional-requirements)
6. [Implementation Phases](#6-implementation-phases)

---

## 1. Overview

Bare Metal Sega Genesis is a high-performance Sega Genesis / Mega Drive emulator for the Raspberry Pi that runs directly on the hardware without a host operating system. By eliminating OS overhead, the emulator achieves lower latency and more predictable performance than OS-hosted emulators.

The project is modeled after the Bare Metal Commodore 64 project (bmc64), which runs the VICE emulator on bare-metal Raspberry Pi hardware. Bare Metal Sega Genesis uses the **Genesis-Plus-GX-Wide** emulator core (via the libretro API) and the **Circle** bare-metal C++ programming environment for Raspberry Pi hardware abstraction.

---

## 2. Scope

This document covers all functionality to be delivered across the implementation phases of Bare Metal Sega Genesis. The system is a standalone bare-metal application with no dependency on Linux, Windows, or any OS distribution. All storage, input, output, and timing are managed directly through the Circle hardware abstraction layer.

---

## 3. System Architecture

The system is composed of three primary layers:

### 3.1 Hardware Abstraction Layer — Circle

Circle (https://github.com/rsta2/circle) provides a bare-metal C++ programming environment for the Raspberry Pi. It supplies drivers and abstractions for:

- SD card storage (FAT32 filesystem)
- USB host (HID controllers, keyboards)
- HDMI video framebuffer
- PWM / I2S audio
- Timers and interrupts
- GPIO

All hardware access in Bare Metal Sega Genesis is performed through Circle APIs. No Linux kernel, device tree, or userspace libraries are used at runtime.

### 3.2 Emulation Core — Genesis-Plus-GX-Wide

Genesis-Plus-GX-Wide (https://github.com/libretro/Genesis-Plus-GX-Wide) provides the Sega Genesis / Mega Drive emulation logic via the **libretro API**. The core handles:

- Motorola 68000 CPU emulation
- Zilog Z80 secondary CPU emulation
- VDP (Video Display Processor) emulation, including widescreen extension support
- YM2612 FM synthesis and SN76489 PSG audio emulation
- Cartridge ROM loading and memory mapping
- Battery-backed SRAM save support

### 3.3 Integration Layer

The integration layer is custom code written for this project. It bridges Circle and the libretro core by:

- Implementing the libretro environment callbacks using Circle APIs (video refresh → framebuffer blit, audio sample → PWM/I2S output, input poll → USB HID state)
- Managing the main emulation loop and frame timing
- Providing the ROM browser menu using Circle's framebuffer
- Handling save state serialization to/from SD card

---

## 4. Functional Requirements

### 4.1 Boot and Startup

#### 4.1.1 Boot Sequence
- On power-on, the Raspberry Pi loads the bare-metal kernel image from the SD card.
- No OS boot process occurs. Circle initializes hardware directly (USB, SD card, HDMI, audio).
- The application displays a splash screen or loading indicator while hardware initialization completes.

#### 4.1.2 Auto-Launch ROM
- If a configuration file specifies an auto-launch ROM path, the emulator loads and starts that ROM immediately after initialization, bypassing the ROM browser.
- If no auto-launch ROM is configured, the ROM browser is presented.

#### 4.1.3 SD Card Requirement
- The system requires an SD card formatted as FAT32 containing:
  - The kernel image
  - A `roms/` directory containing one or more `.md` or `.bin` ROM files
  - An optional `config.txt` configuration file
  - A `saves/` directory (created automatically if absent) for save states and SRAM

---

### 4.2 ROM Browser

#### 4.2.1 ROM Discovery
- On entry to the ROM browser, the system scans the `roms/` directory on the SD card.
- Files with extensions `.md`, `.bin`, and `.smd` are recognized as Sega Genesis ROMs.
- The ROM list is displayed alphabetically by filename.

#### 4.2.2 ROM List Display
- The ROM browser renders a scrollable list of ROM filenames on the HDMI display using the Circle framebuffer.
- The currently highlighted entry is visually distinguished (e.g., different background color or cursor indicator).
- If the list exceeds the visible screen area, it scrolls to keep the highlighted entry visible.

#### 4.2.3 ROM Selection
- The user navigates the list using a connected controller (D-pad up/down) or USB keyboard (arrow keys).
- Pressing the confirm button (controller A button or keyboard Enter) loads and starts the selected ROM.

#### 4.2.4 Empty ROM Directory
- If no compatible ROM files are found in `roms/`, the browser displays an appropriate message (e.g., "No ROMs found. Place .md or .bin files in the /roms directory.").

---

### 4.3 Emulation Core

#### 4.3.1 Core Integration
- Genesis-Plus-GX-Wide is integrated as a libretro core. The integration layer implements all required libretro environment callbacks.
- The emulator core is initialized with the selected ROM path. The ROM file is read from the SD card via Circle's filesystem API and passed to the core.

#### 4.3.2 Emulation Loop
- The main loop calls `retro_run()` once per frame at the Genesis native frame rate (approximately 59.92 fps for NTSC, 50 fps for PAL).
- Frame timing is managed using Circle's hardware timer to maintain accurate cadence and avoid drift.

#### 4.3.3 SRAM / Battery Save Support
- If the loaded ROM uses battery-backed SRAM (e.g., for in-game saves), the SRAM contents are loaded from `saves/<romname>.srm` at ROM load time (if the file exists).
- SRAM is written back to the SD card periodically during emulation and on clean exit or ROM change.

#### 4.3.4 Region Support
- The emulator supports NTSC (60 Hz) and PAL (50 Hz) region modes.
- The active region is configurable (see 4.9).
- Auto-detection of region from ROM header is supported as the default.

---

### 4.4 Video Output

#### 4.4.1 Framebuffer Output
- The emulated Genesis video output is rendered to the Circle HDMI framebuffer each frame.
- The output resolution targets a 1080p or 720p HDMI mode configured at startup.

#### 4.4.2 Scaling
- The Genesis native resolution (320×224 or 256×224) is scaled to fit the output display.
- The default scaling mode is **integer scale** (nearest-neighbor) to preserve pixel sharpness.
- An optional **full-screen stretch** mode (aspect-ratio corrected) is configurable.

#### 4.4.3 Widescreen Support
- Genesis-Plus-GX-Wide supports an extended horizontal resolution mode (up to 320 wide plus extra columns). This widescreen mode is exposed as a configurable option (see 4.9).

#### 4.4.4 Frame Timing
- Video output is synchronized to the display refresh to minimize tearing. VSync behavior is dependent on Circle framebuffer capabilities for the target Raspberry Pi model.

---

### 4.5 Audio Output

#### 4.5.1 Audio Rendering
- The Genesis-Plus-GX-Wide core produces mixed audio samples (YM2612 FM + SN76489 PSG) via the libretro `retro_audio_sample_batch` callback.
- The integration layer routes these samples to the Raspberry Pi audio hardware via Circle (HDMI audio or PWM analog output).

#### 4.5.2 Sample Rate
- The core produces audio at approximately 44,100 Hz stereo. The Circle audio driver is configured to match this sample rate.

#### 4.5.3 Audio Latency
- The audio buffer is sized to balance latency against underrun risk. The target audio latency is ≤ 50 ms.

#### 4.5.4 Audio Output Selection
- Audio is output via HDMI by default.
- Analog audio output (3.5mm jack via PWM) is a configurable alternative for Raspberry Pi models that support it.

---

### 4.6 Controller Input

#### 4.6.1 USB Controller Support
- USB HID gamepads are supported via the Circle USB host stack.
- Up to two controllers are supported simultaneously (player 1 and player 2).
- Controller input is polled each frame via the libretro `retro_input_poll` / `retro_input_state` callbacks.

#### 4.6.2 Genesis Button Mapping
- The default mapping for a standard USB gamepad maps to the Genesis 6-button layout:

| Genesis Button | Default USB Gamepad Mapping |
|---|---|
| D-Pad Up | D-Pad Up |
| D-Pad Down | D-Pad Down |
| D-Pad Left | D-Pad Left |
| D-Pad Right | D-Pad Right |
| A | Button 0 (typically ×/A) |
| B | Button 1 (typically ○/B) |
| C | Button 2 (typically □/X) |
| X | Button 3 (typically △/Y) |
| Y | Button 4 (L1) |
| Z | Button 5 (R1) |
| Start | Start button |
| Mode | Select button |

#### 4.6.3 USB Keyboard Support
- A USB keyboard is recognized as a fallback input device for ROM browser navigation and menu interaction.
- Keyboard-to-Genesis mapping is configurable for players who wish to use a keyboard for gameplay.

#### 4.6.4 Menu Hotkey
- A controller hotkey combination (e.g., Start + Select simultaneously) opens the in-emulation menu without exiting the game (see 4.8).

---

### 4.7 Save States

#### 4.7.1 Save State Creation
- The user can save the complete emulator state (CPU registers, memory, VDP state, audio state) to a file on the SD card.
- Save state files are stored in `saves/<romname>.state<N>` where N is the save slot number (1–4).

#### 4.7.2 Save State Load
- The user can load a previously saved state from any occupied slot, restoring the emulator to the exact state at save time.

#### 4.7.3 Save/Load Interface
- Save and load operations are accessible via the in-emulation menu (see 4.8).
- The menu displays which slots are occupied and the date/time of each save.

#### 4.7.4 Save State Storage
- Save state files are stored on the SD card in the `saves/` directory.
- If a save state file is absent for a given slot, that slot is shown as empty.

---

### 4.8 In-Emulation Menu

#### 4.8.1 Menu Activation
- The in-emulation menu is activated by pressing the designated hotkey combination (see 4.6.4) during gameplay.
- Activating the menu pauses emulation; the last rendered frame remains visible behind the menu overlay.

#### 4.8.2 Menu Options
The in-emulation menu provides the following options:

| Option | Description |
|---|---|
| Resume Game | Dismiss the menu and resume emulation |
| Save State | Save current emulator state to a selected slot |
| Load State | Load a saved state from a selected slot |
| Reset Game | Perform a soft reset of the emulated console |
| Settings | Open the settings screen (see 4.9) |
| Return to ROM Browser | Stop the current game and return to the ROM browser |

#### 4.8.3 Menu Navigation
- The user navigates the menu using the controller D-pad and confirm/cancel buttons, or USB keyboard arrow keys and Enter/Escape.

---

### 4.9 Configuration

#### 4.9.1 Configuration File
- System-level settings are stored in `config.txt` on the SD card root.
- The file is parsed at startup. If the file is absent, all settings use their defaults.
- Settings are editable via the in-emulation menu Settings screen and written back to `config.txt`.

#### 4.9.2 Settings

| Setting | Type | Default | Description |
|---|---|---|---|
| auto_launch_rom | String | (none) | Path to ROM to launch automatically at boot |
| region | Enum (auto, ntsc, pal) | auto | Genesis region / refresh rate |
| video_scale | Enum (integer, stretch) | integer | Video scaling mode |
| widescreen | Boolean | false | Enable Genesis-Plus-GX-Wide extended horizontal resolution |
| audio_output | Enum (hdmi, analog) | hdmi | Audio output device |
| audio_latency_ms | Integer | 40 | Target audio buffer latency in milliseconds |
| controller_1_map | String | (default) | Button mapping for player 1 controller |
| controller_2_map | String | (default) | Button mapping for player 2 controller |
| menu_hotkey | String | start+select | Controller hotkey to open the in-emulation menu |

---

## 5. Non-Functional Requirements

### 5.1 Target Hardware
- Primary target: Raspberry Pi 4 Model B.
- Secondary target: Raspberry Pi 3 Model B / B+.
- Raspberry Pi 5 support is a stretch goal for a later phase.

### 5.2 Performance
- The emulator must sustain full-speed emulation (59.92 fps NTSC / 50 fps PAL) on the Raspberry Pi 4 without frame drops under normal gameplay conditions.
- The emulation loop must complete within the per-frame time budget (~16.7 ms at 59.92 fps) including video blit, audio output, and input polling.

### 5.3 Input Latency
- End-to-end input latency (controller button press to pixel change on screen) must be minimized. Target: ≤ 3 frames (≤ 50 ms at 60 fps).
- Bare-metal execution eliminates OS scheduling jitter, which is a primary motivation for this architecture.

### 5.4 No Operating System Dependency
- The compiled kernel image must boot and run without any Linux distribution, init system, or kernel modules.
- All required drivers and runtime support are provided by Circle and statically linked into the kernel image.

### 5.5 ROM Compatibility
- The emulator targets compatibility with the commercial Sega Genesis / Mega Drive ROM catalog as provided by the Genesis-Plus-GX-Wide core.
- ROM compatibility is inherited from the upstream Genesis-Plus-GX-Wide core and is not extended or restricted by this project.

### 5.6 Storage
- The SD card must be formatted as FAT32.
- The system does not require a partition beyond the single FAT32 volume used by Circle.

### 5.7 Build Environment
- The project is built using the Circle-supported ARM cross-compilation toolchain (arm-none-eabi or aarch64-none-elf depending on target).
- The build system produces a `kernel.img` (Pi 3) or `kernel8.img` (Pi 4) suitable for placement on the SD card boot partition.

### 5.8 Open Source Compliance
- Genesis-Plus-GX-Wide is distributed under a non-commercial license. This project must comply with all upstream licensing requirements.
- Circle is distributed under the GNU GPL v3. The Bare Metal Sega Genesis project must comply with Circle's license terms.

---

## 6. Implementation Phases

### Phase 1 — Core Integration and Basic Playability

**Goal:** Achieve basic end-to-end emulation: load a ROM, render video, produce audio, and accept controller input on bare-metal Raspberry Pi hardware.

| Deliverable | Description |
|---|---|
| Circle project scaffold | Bare-metal project structure using Circle, targeting Raspberry Pi 4 |
| libretro core integration | Genesis-Plus-GX-Wide compiled and linked against the integration layer |
| ROM loading | Load a hard-coded or config-specified ROM from the SD card at boot |
| Video output | Blit the libretro framebuffer to the HDMI display each frame |
| Audio output | Route libretro audio samples to HDMI audio via Circle |
| Controller input | Poll a single USB HID gamepad and pass input state to the libretro core |
| Frame timing | Maintain accurate 60/50 Hz frame cadence using Circle timers |
| SRAM persistence | Load and save battery-backed SRAM to/from SD card |

**Success Criteria:** A Sega Genesis game is playable from start to finish on bare-metal Raspberry Pi hardware.

---

### Phase 2 — ROM Browser and Save States

**Goal:** Add user-facing features to make the system usable without code changes between play sessions.

| Deliverable | Description |
|---|---|
| ROM browser | Scrollable list of ROMs from the `roms/` directory with controller navigation |
| In-emulation menu | Overlay menu accessible via hotkey during gameplay |
| Save states | Save and load emulator state to/from up to 4 slots per ROM |
| Configuration file | Parse `config.txt` at startup; support auto-launch, region, and scale settings |
| Settings screen | In-menu settings screen that writes back to `config.txt` |

**Success Criteria:** The user can select a ROM from the browser, play it, save and restore progress, and configure basic options — all without modifying files on the SD card except through the in-emulation menu.

---

### Phase 3 — Polish and Expanded Support

**Goal:** Refine the user experience, broaden hardware support, and add quality-of-life features.

| Deliverable | Description |
|---|---|
| Raspberry Pi 3 support | Validate and tune performance on Raspberry Pi 3 Model B/B+ |
| Two-player input | Support a second USB controller for two-player games |
| Widescreen mode | Expose Genesis-Plus-GX-Wide widescreen extension as a configurable option |
| Analog audio output | Support 3.5mm analog audio output as an alternative to HDMI audio |
| Controller remapping | User-configurable button mapping via the settings screen |
| PAL region support | Validate PAL timing (50 Hz) and region auto-detection |
| Splash screen | Branded boot screen during Circle hardware initialization |
| GitHub repository | Publish source and build instructions to https://github.com/mjaydedecker/Bare-Metal-Sega-Genesis |

**Success Criteria:** The project is publicly released, builds cleanly from source, and provides a complete, documented experience for both Raspberry Pi 3 and Pi 4 users.
