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

Off-the-shelf DB9-to-GPIO HAT/adapter. The HAT handles 5V↔3.3V level shifting
(real Genesis pads are 5V logic; the Pi GPIO is 3.3V and **not** 5V-tolerant) and
shares ground. The firmware is kept agnostic to the exact board via a single
pin-map abstraction, so a DIY harness could be substituted later by changing one
constant.

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

### M0 — HAT pin-map & board abstraction
Pick/obtain the DB9 HAT and document its GPIO assignment (6 data + 1 SELECT per
port × 2 ports = 14 lines). Capture it as a single `BoardPinMap` struct/header so
a DIY harness can swap in later. Confirm the HAT handles level shifting and
common ground.
**Deliverable:** documented pin map + `BoardPinMap` constant.

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

- DIY discrete-wiring bring-up (the pin-map abstraction leaves the door open).
- Multitap / more than two ports.
- Non-Sega DB9 pads beyond detecting and ignoring the Atari/SMS signature.
