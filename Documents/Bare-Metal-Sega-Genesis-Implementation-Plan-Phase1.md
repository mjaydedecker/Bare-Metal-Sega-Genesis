# Implementation Plan
# Bare Metal Sega Genesis — Phase 1: Core Integration and Basic Playability

**Version:** 1.0
**Date:** 2026-04-06
**Based on:** Bare-Metal-Sega-Genesis-FSD.md v1.0 — Phase 1

---

## Table of Contents

1. [Project Structure](#1-project-structure)
2. [Dependency Graph Between Layers](#2-dependency-graph-between-layers)
3. [Milestones](#3-milestones)
   - M1: Repository and Build Scaffold
   - M2: Circle Kernel Bootstrap
   - M3: SD Card and ROM Loading
   - M4: libretro Core Integration
   - M5: Video Output
   - M6: Audio Output
   - M7: Controller Input
   - M8: Frame Timing
   - M9: SRAM Persistence
4. [Key Architectural Decisions](#4-key-architectural-decisions)
5. [Critical Files](#5-critical-files)

---

## 1. Project Structure

```
Bare-Metal-Sega-Genesis/
├── src/
│   ├── kernel.h                        ← CKernel class declaration
│   ├── kernel.cpp                      ← CKernel::Initialize() and CKernel::Run()
│   ├── main.cpp                        ← Circle entry point; instantiates CKernel
│   ├── libretro/
│   │   ├── environment.h               ← retro_environment_t callback declaration
│   │   ├── environment.cpp             ← RETRO_ENVIRONMENT_* handler implementations
│   │   ├── callbacks.h                 ← video/audio/input callback declarations
│   │   └── callbacks.cpp               ← retro_video_refresh, retro_audio_sample_batch,
│   │                                      retro_input_poll, retro_input_state implementations
│   ├── video/
│   │   ├── display.h
│   │   └── display.cpp                 ← Circle framebuffer wrapper; blit with integer scaling
│   ├── audio/
│   │   ├── audio_driver.h
│   │   └── audio_driver.cpp            ← Circle HDMI audio wrapper; ring buffer management
│   ├── input/
│   │   ├── gamepad.h
│   │   └── gamepad.cpp                 ← Circle USB HID gamepad handler; button state map
│   ├── storage/
│   │   ├── sdcard.h
│   │   └── sdcard.cpp                  ← Circle FatFs wrapper; ROM read, SRAM read/write
│   └── timing/
│       ├── frametimer.h
│       └── frametimer.cpp              ← Circle hardware timer; 59.92 / 50 Hz cadence
├── libs/
│   ├── circle/                         ← git submodule: github.com/rsta2/circle
│   └── genesis-plus-gx-wide/           ← git submodule: github.com/libretro/Genesis-Plus-GX-Wide
├── Makefile                            ← top-level build; produces kernel8.img (RPi 4)
├── Rules.mk                            ← shared compile flags and include paths
├── .gitmodules                         ← submodule declarations
└── Documents/
    ├── Bare-Metal-Sega-Genesis-Project-Idea.md
    ├── Bare-Metal-Sega-Genesis-FSD.md
    └── Bare-Metal-Sega-Genesis-Implementation-Plan-Phase1.md
```

---

## 2. Dependency Graph Between Layers

```
Circle HAL (USB, SD, HDMI, timers)
        │
        ▼
 ┌──────────────────────────────────────────┐
 │           Integration Layer              │
 │  storage/sdcard  →  libretro/callbacks   │
 │  video/display   →  libretro/callbacks   │
 │  audio/driver    →  libretro/callbacks   │
 │  input/gamepad   →  libretro/callbacks   │
 │  timing/frametimer                       │
 │  libretro/environment                    │
 └──────────────────────────────────────────┘
        │
        ▼
 Genesis-Plus-GX-Wide libretro core
 (retro_init / retro_load_game / retro_run)
        │
        ▼
   kernel.cpp CKernel::Run() — main loop
```

Each layer depends only downward. `kernel.cpp` is the top-level orchestrator: it initializes Circle, initializes the integration layer, loads the ROM, then drives `retro_run()` in a loop. The libretro core calls back upward through the registered callback pointers — it never directly touches Circle APIs.

---

## 3. Milestones

---

### Milestone 1 — Repository and Build Scaffold

**Goal:** A repository that compiles to a bootable (blank-screen) `kernel8.img` for Raspberry Pi 4.

**Tasks:**

1. Create the GitHub repository at https://github.com/mjaydedecker/Bare-Metal-Sega-Genesis.
2. Add Circle as a git submodule at `libs/circle/`:
   ```
   git submodule add https://github.com/rsta2/circle libs/circle
   ```
3. Add Genesis-Plus-GX-Wide as a git submodule at `libs/genesis-plus-gx-wide/`:
   ```
   git submodule add https://github.com/libretro/Genesis-Plus-GX-Wide libs/genesis-plus-gx-wide
   ```
4. Write `Rules.mk` defining:
   - Cross-compiler prefix (`aarch64-none-elf-` for RPi 4 64-bit)
   - Include paths for Circle headers and Genesis-Plus-GX-Wide headers
   - Compile flags: `-O2 -ffreestanding -nostdinc -nostdlib -DRASPPI=4`
   - libretro core compile flags as required by Genesis-Plus-GX-Wide
5. Write `Makefile` that:
   - Builds the Circle library (invoke Circle's own `Makefile`)
   - Compiles Genesis-Plus-GX-Wide source files into a static archive `libgenesis.a`
   - Compiles all `src/` files
   - Links everything into `kernel8.img` using Circle's linker script
6. Write minimal `src/main.cpp` (Circle standard entry point — instantiates `CKernel` and calls `Initialize()` / `Run()`).
7. Write minimal `src/kernel.h` / `src/kernel.cpp` with empty `Initialize()` returning `true` and `Run()` that loops forever.
8. Verify `kernel8.img` is produced and boots to a stable (blank) screen on Raspberry Pi 4 hardware.

---

### Milestone 2 — Circle Kernel Bootstrap

**Goal:** Kernel initializes all required Circle subsystems without crashing.

**Tasks:**

1. In `CKernel::Initialize()`, initialize Circle subsystems in dependency order:
   - `CActLED` — activity LED for visual boot confirmation
   - `CTimer` — system timer (required by most other subsystems)
   - `CLogger` — optional serial logger over UART for development diagnostics
   - `CScheduler` — cooperative task scheduler (required by USB stack)
   - `CDWHCIDevice` — USB host controller (for gamepad input)
   - `FATFS` / `CFATFileSystem` — SD card filesystem for ROM and SRAM access
   - `CBcmFrameBuffer` — HDMI framebuffer for video output
   - `CPWMSoundBaseDevice` or HDMI audio device — audio output
2. Log each subsystem initialization success/failure via `CLogger`.
3. Blink the activity LED on successful `Initialize()` as a hardware sanity check.
4. In `CKernel::Run()`, implement a trivial loop that keeps the kernel alive (no emulation yet).
5. Boot on RPi 4 hardware and confirm all subsystems initialize without hang.

**Note:** Circle subsystem initialization order matters — consult Circle sample projects (e.g., `sample/06-defscreen`, `sample/05-usbsimple`) for the correct sequence for each subsystem.

---

### Milestone 3 — SD Card and ROM Loading

**Goal:** Read a ROM file from the SD card into memory.

**Files:** `src/storage/sdcard.h`, `src/storage/sdcard.cpp`

**Tasks:**

1. Implement `SDCard::Mount()` — mounts the FAT32 filesystem via Circle's `FATFS` API.
2. Implement `SDCard::ReadFile(const char* path, uint8_t** outBuffer, size_t* outSize)`:
   - Opens the file using `f_open`
   - Allocates a buffer via `new uint8_t[fileSize]`
   - Reads the full file into the buffer with `f_read`
   - Returns the buffer and size to the caller
   - Returns `false` with a log message if the file is not found or read fails
3. Implement `SDCard::WriteFile(const char* path, const uint8_t* data, size_t size)` — used later for SRAM persistence (M9), stub it now.
4. In `CKernel::Initialize()`, mount the SD card and attempt to read a hard-coded ROM path (e.g., `roms/sonic.md`).
5. Log the ROM filename and file size to confirm successful read.
6. If the ROM file is absent, log an error and halt with a clear message (Phase 2 will add the ROM browser).

---

### Milestone 4 — libretro Core Integration

**Goal:** The Genesis-Plus-GX-Wide core initializes and loads the ROM without crashing.

**Files:** `src/libretro/environment.h`, `src/libretro/environment.cpp`, `src/libretro/callbacks.h`, `src/libretro/callbacks.cpp`

**Tasks:**

1. Register stub implementations of all required libretro callbacks before calling `retro_init()`:
   - `retro_set_environment(environment_callback)`
   - `retro_set_video_refresh(video_refresh_callback)` — stub: no-op for now
   - `retro_set_audio_sample(audio_sample_callback)` — stub: discard samples
   - `retro_set_audio_sample_batch(audio_batch_callback)` — stub: discard samples
   - `retro_set_input_poll(input_poll_callback)` — stub: no-op
   - `retro_set_input_state(input_state_callback)` — stub: return 0 (no input)
2. Implement `environment_callback(unsigned cmd, void* data)` to handle the minimum required `RETRO_ENVIRONMENT_*` commands:
   - `RETRO_ENVIRONMENT_GET_LOG_INTERFACE` — return a logging function that forwards to `CLogger`
   - `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT` — record the pixel format (expect `RETRO_PIXEL_FORMAT_RGB565` or `XRGB8888`)
   - `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` — return `"/"` (SD card root)
   - `RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY` — return `"/saves/"`
   - All unhandled commands: return `false`
3. Call `retro_init()`.
4. Call `retro_load_game()` with the ROM buffer read in M3 (populate `retro_game_info` struct with path, data pointer, and size).
5. Verify `retro_load_game()` returns `true`. Log success or failure.
6. Call `retro_get_system_av_info()` and log the reported geometry (base width/height, max width/height, FPS) and audio sample rate. These values drive video scaling (M5) and audio buffer sizing (M6).

---

### Milestone 5 — Video Output

**Goal:** The emulated Genesis video is visible on the HDMI display each frame.

**Files:** `src/video/display.h`, `src/video/display.cpp`, update `src/libretro/callbacks.cpp`

**Tasks:**

1. In `CKernel::Initialize()`, initialize `CBcmFrameBuffer` targeting a 1920×1080 HDMI mode with a 32-bit color depth back buffer.
2. Implement `Display::Blit(const void* frameData, unsigned width, unsigned height, size_t pitch, PixelFormat fmt)`:
   - Computes integer scale factor: `scale = min(1920 / width, 1080 / height)`
   - Computes centered destination offset: `offsetX = (1920 - width*scale) / 2`, `offsetY = (1080 - height*scale) / 2`
   - Iterates over each source pixel and writes it (nearest-neighbor scaled) to the framebuffer
   - Converts pixel format to the framebuffer's native ARGB8888 if the core outputs RGB565
3. Implement the `video_refresh_callback(const void* data, unsigned width, unsigned height, size_t pitch)` libretro callback to call `Display::Blit()`.
4. In `CKernel::Run()`, call `retro_run()` in a loop and verify the Genesis video appears on screen.

**Pixel format note:** Genesis-Plus-GX-Wide can be configured to output RETRO_PIXEL_FORMAT_RGB565 or RETRO_PIXEL_FORMAT_XRGB8888. Accept whichever the core requests via `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT` and convert accordingly in `Display::Blit`.

---

### Milestone 6 — Audio Output

**Goal:** Game audio is audible through the HDMI output each frame.

**Files:** `src/audio/audio_driver.h`, `src/audio/audio_driver.cpp`, update `src/libretro/callbacks.cpp`

**Tasks:**

1. Determine the Circle audio mechanism available for HDMI audio on Raspberry Pi 4 (Circle's `CHDMISoundBaseDevice` or `CVCHIQSoundBaseDevice`). Initialize the chosen device in `CKernel::Initialize()` at the sample rate reported by `retro_get_system_av_info()` (typically 44,100 Hz, stereo, 16-bit).
2. Implement `AudioDriver` with an internal ring buffer sized for approximately 40 ms of audio at 44,100 Hz stereo (≈ 3,528 stereo frames):
   - `AudioDriver::Write(const int16_t* samples, size_t frames)` — writes samples into the ring buffer
   - `AudioDriver::Submit()` — transfers buffered samples to the Circle audio device
3. Implement `audio_batch_callback(const int16_t* data, size_t frames)` to call `AudioDriver::Write()`.
4. Implement `audio_sample_callback(int16_t left, int16_t right)` (single-sample variant) to enqueue one stereo frame — some cores use this path intermittently.
5. Call `AudioDriver::Submit()` once per frame in `CKernel::Run()` after `retro_run()`.
6. Verify game audio is audible and not severely glitched or stuttering on hardware.

**Buffer sizing note:** Undersized buffers cause audio dropouts; oversized buffers add latency. Start with 40 ms and tune on hardware. The ring buffer must handle the case where the core produces a variable number of samples per `retro_run()` call.

---

### Milestone 7 — Controller Input

**Goal:** A USB gamepad controls the emulated Genesis console.

**Files:** `src/input/gamepad.h`, `src/input/gamepad.cpp`, update `src/libretro/callbacks.cpp`

**Tasks:**

1. Initialize Circle's USB HID gamepad device in `CKernel::Initialize()` (via `CUSBGamePadDevice` or the Circle USB gamepad framework). Register a report handler that updates a cached button state struct.
2. Implement `Gamepad::PollState()` — called once per frame to snapshot the current button and D-pad state from the Circle USB report.
3. Implement `input_poll_callback()` to call `Gamepad::PollState()` and cache the result.
4. Implement `input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id)`:
   - For port 0, `RETRO_DEVICE_JOYPAD`: map the libretro button ID (`RETRO_DEVICE_ID_JOYPAD_*`) to the corresponding cached USB HID button using the default mapping from FSD §4.6.2.
   - Return `1` if the button is pressed, `0` otherwise.
   - For port 1 and all other device types: return `0` (single-player in Phase 1).
5. Verify D-pad, A/B/C, and Start buttons work in a game on hardware.

**Default button mapping (Phase 1, single controller):**

| `RETRO_DEVICE_ID_JOYPAD_*` | USB HID Source |
|---|---|
| UP / DOWN / LEFT / RIGHT | D-pad hat |
| A | Button index 0 |
| B | Button index 1 |
| X (Genesis C) | Button index 2 |
| Y (Genesis X) | Button index 3 |
| L (Genesis Y) | Button index 4 |
| R (Genesis Z) | Button index 5 |
| START | Start button |
| SELECT (Genesis Mode) | Select button |

---

### Milestone 8 — Frame Timing

**Goal:** The emulation loop runs at accurate Genesis speed (59.92 Hz NTSC) without drift.

**Files:** `src/timing/frametimer.h`, `src/timing/frametimer.cpp`, update `src/kernel.cpp`

**Tasks:**

1. Implement `FrameTimer::Start()` — records the current Circle high-resolution timer value as the frame start.
2. Implement `FrameTimer::WaitForNextFrame()`:
   - Computes elapsed time since `Start()` using `CTimer::GetClockTicks()`
   - Busy-waits or yields until the full frame period has elapsed
   - Frame period = `1,000,000 µs / fps` where `fps` is taken from `retro_get_system_av_info()` (e.g., 59.9227 Hz → 16,680 µs per frame)
3. Update `CKernel::Run()` to call `FrameTimer::Start()` at the top of each iteration and `FrameTimer::WaitForNextFrame()` at the bottom, after video blit and audio submit.
4. Measure actual frame time on hardware (log every 300 frames) and verify it is within ±0.5% of the target.

**Timing note:** Circle's `CTimer` provides microsecond-resolution ticks. Busy-waiting for the remainder of a frame is acceptable in a bare-metal context where there is no OS scheduler to contend with. If `retro_run()` + blit + audio take longer than one frame period, log a warning but do not attempt to skip frames in Phase 1.

---

### Milestone 9 — SRAM Persistence

**Goal:** Battery-backed in-game saves (SRAM) are preserved across power cycles.

**Files:** `src/storage/sdcard.cpp` (extend from M3), update `src/libretro/environment.cpp` and `src/kernel.cpp`

**Tasks:**

1. After `retro_load_game()` succeeds, call `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` and `retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)`:
   - If size > 0, the ROM uses battery-backed SRAM.
   - Derive the SRAM path: `saves/<romname>.srm` where `<romname>` is the ROM filename without extension.
   - If the `.srm` file exists on the SD card, read it and `memcpy` its contents into the SRAM buffer pointer.
2. Implement periodic SRAM flush in `CKernel::Run()`:
   - Every 300 frames (~5 seconds at 60 fps), if the ROM uses SRAM, write the current SRAM buffer contents to `saves/<romname>.srm` via `SDCard::WriteFile()`.
3. On clean exit (if a soft-reset or future menu exit path is added), flush SRAM immediately before halting.
4. If the ROM has no SRAM (size == 0), skip all SRAM logic silently.
5. Verify with a known SRAM-using ROM (e.g., Phantasy Star IV) that in-game saves persist across a power cycle.

---

## 4. Key Architectural Decisions

### 4.1 Static Linking vs. Dynamic libretro Core

Genesis-Plus-GX-Wide is compiled as a **static library** (`libgenesis.a`) and linked directly into `kernel8.img`. There is no dynamic linker in a bare-metal environment, so a dynamically loaded `.so` core (as used by RetroArch on Linux) is not applicable here. All libretro `retro_*` symbols are resolved at link time.

### 4.2 64-bit vs. 32-bit Kernel (RPi 4)

The Raspberry Pi 4 supports both AArch32 and AArch64 execution. Circle supports both modes. This project targets **AArch64 (64-bit)** on RPi 4 for better performance, producing `kernel8.img`. Genesis-Plus-GX-Wide must be compiled with the same AArch64 toolchain (`aarch64-none-elf-`).

### 4.3 Pixel Format

The Genesis-Plus-GX-Wide core will request a pixel format via `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT`. Accept `RETRO_PIXEL_FORMAT_RGB565` as the preferred format (lower bandwidth, sufficient for Genesis output). If the core requests `XRGB8888`, accept that too and blit without conversion. Store the accepted format in a global so `video_refresh_callback` knows how to convert to the framebuffer's native ARGB8888.

### 4.4 Audio: HDMI vs. Analog

Phase 1 targets **HDMI audio** exclusively. Circle's HDMI audio device requires the VCHIQ interface, which is available on RPi 4. Analog PWM audio is deferred to Phase 3 (FSD §4.5.4). If HDMI audio proves difficult to initialize under Circle on RPi 4, fall back to Circle's `CPWMSoundBaseDevice` for analog as a Phase 1 contingency.

### 4.5 Frame Timing: Busy-Wait

A bare-metal kernel has no scheduler preemption, so the simplest correct approach is a busy-wait loop in `FrameTimer::WaitForNextFrame()`. This is the same approach used by other bare-metal emulators (including bmc64). CPU cycles "wasted" busy-waiting are irrelevant in this context — the alternative (a sleep/yield) would require a more complex Circle scheduler integration without meaningful benefit in Phase 1.

---

## 5. Critical Files

| File | Role |
|---|---|
| `src/kernel.cpp` | Top-level orchestrator — initializes subsystems, loads ROM, drives main loop |
| `src/libretro/callbacks.cpp` | Bridges libretro callbacks to Circle subsystems (video, audio, input) |
| `src/libretro/environment.cpp` | Handles `RETRO_ENVIRONMENT_*` commands from the core |
| `src/video/display.cpp` | Integer-scale blit from libretro framebuffer to Circle HDMI framebuffer |
| `src/audio/audio_driver.cpp` | Ring buffer and submission to Circle HDMI audio device |
| `src/input/gamepad.cpp` | USB HID state polling and button mapping |
| `src/storage/sdcard.cpp` | ROM read and SRAM read/write via Circle FatFs |
| `src/timing/frametimer.cpp` | Hardware-timer-based frame cadence |
| `Makefile` | Builds Circle, compiles Genesis-Plus-GX-Wide, links kernel8.img |
| `libs/circle/` | Circle bare-metal environment (git submodule) |
| `libs/genesis-plus-gx-wide/` | Genesis emulation core (git submodule) |
