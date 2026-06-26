# GPIO Real Sega Controllers — Design

**Date:** 2026-06-25
**Status:** Design (approved milestone breakdown; pending spec review)

## Goal

Let real Sega Genesis / Mega Drive controllers (DB9, 3- and 6-button) drive the
emulator through the Raspberry Pi GPIO pins, coexisting with the existing USB
gamepad support, across two ports (Player 1 + Player 2).

## Architecture anchor

A DB9 Sega pad is a SELECT-multiplexed button latch. The console (us) drives the
**SELECT** line (DB9 pin 7) and reads six data lines (pins 1, 2, 3, 4, 6, 9).
The meaning of the data lines changes with the SELECT level, and rapidly pulsing
SELECT exposes the 6-button extras (X / Y / Z / Mode).

The existing input pipeline already isolates the physical source from everything
downstream:

- `Gamepad` (src/input/gamepad.{h,cpp}) caches a per-port `GP_*` button bitmask.
- The **pure, host-tested** `joypad_state()` (src/input/joypad_map.{h,cpp}) maps
  `GP_*` bits → libretro joypad IDs through the active (remappable) `ButtonMap`.
- Menus, hotkeys, remap UI, and the core all read through that bitmask
  (`Buttons(port)` / `MenuButtons()`).

**Design principle:** GPIO becomes a *second producer of the same per-port
`GP_*` bitmask*. Once the GPIO source produces that mask, the entire downstream
pipeline works unchanged. No menu, hotkey, remap, or core-mapping code changes.

### Platform primitives (confirmed available in Circle)

- `CGPIOPin` — `SetMode(GPIOModeOutput | GPIOModeInputPullUp)`, `Read()`,
  `Write()`, bulk `SetModeAll(inputMask, outputMask)`.
- `CTimer::SimpleusDelay()` / `usDelay()` / `nsDelay()` for SELECT-toggle
  settling; `GetClockTicks()` for the ~1.5 ms 6-button reset window.

No new hardware-access primitives are required.

### GP_* bit space

The current 8-button `GP_*` set — A, B, X, Y, LB, RB, SELECT, START — is exactly
enough to carry the Genesis face/control set: A, B, C, X, Y, Z, Start, Mode. The
d-pad bits (UP/DOWN/LEFT/RIGHT) already exist. No widening of the bitmask is
needed.

## Hardware interface

DB9 jacks wired **directly** to the Pi GPIO header — no level shifters, no
voltage dividers, and no external shift registers/multiplexers. This is made
safe by **powering the controller at 3.3V** (DB9 pin 5 → the Pi 3.3V rail)
rather than 5V:

- A Genesis pad is an *active* device — it contains its own 74HC157-class
  multiplexer powered from pin 5, and that mux **actively drives** the data
  lines to its VCC rail. Powered at 3.3V, the outputs swing 0–3.3V, which is
  in-spec for the Pi's non-5V-tolerant GPIO. (Powered at 5V they'd drive ~5V
  into the GPIO — the thing to avoid; that's the out-of-spec path some Pico
  builds get away with only because the RP2040's input clamp diodes survive it.
  The Pi SoC GPIO is less forgiving and soldered-down — do not replicate it.)
- No shift registers are added: the Sega protocol is **parallel select** via the
  single SELECT line (unlike NES/SNES serial 4021 shift registers). The mux is
  already inside the pad; we just toggle SELECT and read 6 lines.
- The 74HC157 runs fine at 3.3V VCC, and the Pi's 3.3V SELECT output clears its
  logic-high threshold (0.7×3.3 ≈ 2.3V) comfortably.

**Electrical contract for the HAT:** DB9 pin 5 = **+3.3V** (NOT 5V), pin 8 = GND,
data/SELECT lines straight to the assigned GPIOs. Data inputs rely on the SoC
**internal pull-ups** (`GPIOModeInputPullUp`); the pad pulls a line low when a
button/direction is active.

The physical board is **out of scope for this project** — it is being spun off as
a **separate "proper HAT" hardware project** built to the pin assignment and
electrical contract defined here. This project's firmware is kept agnostic to the
board via a single `BoardPinMap` constant, so the HAT (or a bring-up jumper
harness) can change without touching the driver.

## GPIO pin assignment (Pi 2, 40-pin header, BCM numbering)

Each port = 1 SELECT output + 6 data inputs. Data lines `D0..D5` map to DB9 pins
**1, 2, 3, 4, 6, 9** (Up, Down, Left, Right, TL[B/A], TR[C/Start]); DB9 pin 7 =
SELECT, pin 5 = +3.3V, pin 8 = GND.

| Signal               | DB9 pin | Port 1 | Port 2 |
|----------------------|---------|--------|--------|
| SELECT (output)      | 7       | GPIO4  | GPIO11 |
| D0 — Up              | 1       | GPIO5  | GPIO12 |
| D1 — Down            | 2       | GPIO6  | GPIO13 |
| D2 — Left            | 3       | GPIO7  | GPIO16 |
| D3 — Right           | 4       | GPIO8  | GPIO17 |
| D4 — TL (B / A)      | 6       | GPIO9  | GPIO22 |
| D5 — TR (C / Start)  | 9       | GPIO10 | GPIO23 |

**Reserved / deliberately avoided:**
- **GPIO18–21** — Circle I2S DAC (PCM_CLK/FS/DIN/DOUT); live `audio_output=i2s`
  option, must stay clear.
- **GPIO14/15** — UART (Circle serial init).
- **GPIO2/3** — I2C, kept free for the deferred I2C-config DAC work.
- **GPIO0/1** — HAT ID EEPROM (ID_SD/ID_SC), reserved for the separate HAT.
- **GPIO24/25/26/27** — left spare (status LED, multitap, future use).

This table is the authoritative interface the separate HAT project builds to.
Individual pin choices within the available set are not load-bearing for the
firmware (they live in `BoardPinMap`); the *reservations* above are.

## Protocol

Support 3- and 6-button pads together, with live detection:

- **3-button read:** SELECT=HIGH → {Up, Down, Left, Right, B, C};
  SELECT=LOW → {Up, Down, (Left/Right forced low), A, Start}. The Left+Right
  both reading low while SELECT=LOW is the signature that distinguishes a Genesis
  3-button pad from an Atari / Master System pad.
- **6-button read:** pulsing SELECT multiple times; on the 3rd SELECT=LOW the
  data lines read all-low (the 6-button signature), then the following
  SELECT=HIGH exposes Mode, X, Y, Z. The pad's internal cycle counter resets
  after a ~1.5 ms idle on SELECT.

## Milestones

### M0 — Pin map & board abstraction
Encode the GPIO pin assignment above (14 lines: 6 data + 1 SELECT per port × 2)
as a single `BoardPinMap` struct/header — the one place that knows physical pins,
so the HAT or a bring-up jumper harness can change without touching the driver.
The physical board itself is **out of scope** (separate HAT project, built to the
electrical contract + pin table in this spec).
**Deliverable:** `BoardPinMap` constant matching the pin-assignment table.

### M1 — Pure protocol decoder (host-tested, TDD)
New pure module `sega_pad.{h,cpp}` (no Circle deps), mirroring the `joypad_map`
pure/host-tested pattern. Input: the ordered raw pin samples across SELECT
phases. Output: decoded Genesis buttons + detected pad type
(none / Atari-signature / 3-button / 6-button), expressed as a `GP_*` bitmask
plus a pad-type enum. Table-driven host tests with sample-sequence vectors for
each case.
**Deliverable:** decoder + passing host tests, zero hardware.

### M2 — GPIO reader + timing state machine (Circle)
`gpio_pads.{h,cpp}`: owns the `CGPIOPin`s per the M0 pin-map, runs the
SELECT-toggle burst with `usDelay` settling, respects the ~1.5 ms inter-burst
reset window (naturally satisfied by the ~16 ms frame gap — the constraint is
simply *do not double-poll within 1.5 ms*), and feeds samples into the M1
decoder. Mirrors `Gamepad`'s shape: `Buttons(port)`, `IsPresent(port)`,
`RawButtons(port)`. Knows nothing about libretro.

### M3 — Source arbitration (coexist USB + GPIO)
Per-port arbiter that merges the GPIO and USB bitmasks into the single mask the
kernel feeds `joypad_state` / `MenuButtons()`. Wire into the kernel poll loop;
menus/hotkeys/remap untouched because they already read the merged mask.

**Open decision (resolve at this milestone): collision rule.**
Options: (a) OR both sources per port; (b) GPIO-present-wins. **Recommendation:
OR both** — simplest, and a port rarely has both a USB and a DB9 pad
simultaneously, so the practical behavior is identical while avoiding a
presence-priority edge case. Decide when the arbiter is implemented.

### M4 — 6-button + live 3/6 detection → MDPAD type
Map Genesis C / Z / Mode onto the spare `GP_*` bits (A, B, C, X, Y, Z, Start,
Mode fit the 8-bit set exactly). Tie the decoder's detected 3-vs-6 result to the
existing `MDPAD_3B` / `MDPAD_6B` core-type selection — now per-port from live
detection rather than per-ROM.

### M5 — Settings & UX
Default = coexist. Add a "GPIO pad detected" indicator. Genesis pads have no
hotplug event, so presence is inherently polled each frame by signature, which
fits the existing `pad_reconcile` model cleanly.

**Open decision (resolve at this milestone): disable-toggle.**
Whether to add a Settings row to disable GPIO input. **Recommendation: YAGNI —
coexist-always** unless a concrete need appears (e.g. electrical noise on an
unpopulated port producing phantom input). Decide when building the settings row;
if dropped, M5 is just the detection indicator.

### M6 — Hardware verification (Pi 2)
Bench test with real 3- and 6-button pads: full d-pad + all buttons, both ports,
correct 3/6 auto-detect, coexist with a USB pad, latency (the SELECT burst is
microseconds — negligible vs the 16.67 ms frame), and no audio/video regression.
Add as the next hardware-checklist letter.

## Testing strategy

- **M1** is fully host-testable (pure decoder, table-driven vectors) and is the
  correctness core — this is where the protocol logic is proven.
- **M2/M3** are thin Circle integration layers verified on hardware (M6).
- Latency is not expected to be a concern: a full 6-button read is a handful of
  microsecond-scale SELECT edges, far inside the frame budget.

## Out of scope

- **The physical HAT/PCB** — a separate hardware project built to the pin
  assignment + electrical contract in this spec. Bench bring-up may use a jumper
  harness against the same `BoardPinMap`.
- Multitap / more than two ports.
- Non-Sega DB9 pads beyond detecting and ignoring the Atari/SMS signature.
