# Controller Auto-Mapping (Calibration) — Design

**Date:** 2026-06-21
**Status:** approved design, pending implementation plan
**Related:** controller deferred item "controller-agnostic auto-mapping"
([[project-two-controllers]]); `src/input/gamepad.{h,cpp}` (raw HID decode),
`src/input/joypad_map.{h,cpp}` (`GP_*` bits, `pad_bit`, the 8BitDo-specific raw
layout), the per-port `ButtonMap` remap, the settings/Storage subsystem, the
TextCanvas menu screens.

## Summary

Make non-8BitDo USB gamepads work by letting the user **calibrate** a controller
once: a Settings screen prompts "press A… press B…", captures which raw HID button
bit each press sets, and stores the mapping **per controller model (VID:PID)** in
`SD:/controllers.txt`. At runtime the input layer remaps that controller's raw
bits into our logical `GP_*` slots. A pad with no saved calibration falls back to
today's 8BitDo identity mapping, so existing behavior is unchanged.

The face/shoulder `GP_*` bits in `joypad_map.h` are currently raw HID bits
"observed on an 8BitDo," so any other pad's buttons land wrong or dead. This adds a
normalization layer *below* the existing Genesis-button remap (`ButtonMap`), which
is untouched.

## Decisions (from brainstorming)

- **Manual trigger** — a "Calibrate Controller..." Settings entry; no auto-prompt.
- **8 digital buttons only** (A, B, X, Y, L, R, Start, Select); the **D-pad stays
  on the existing hat/analog-axis path** (already generic).
- **Keyed by controller model (VID:PID)** — a calibration follows that model across
  ports.
- **Dedicated file `SD:/controllers.txt`** — calibrations are a many-keyed
  collection, not a single settings struct.
- **Fallback = today's 8BitDo identity mapping** for uncalibrated pads.
- **Calibrates the port-1 pad**; the D-pad drives skip/cancel during capture.

## Feasibility (confirmed)

- `TGamePadState` (Circle `usbgamepad.h`) exposes `unsigned buttons` (raw digital
  bitmask), plus `hats`/`axes` for the D-pad. Capturing "which bit a press sets" is
  direct.
- VID/PID is reachable: `CUSBGamePadDevice` → `GetDevice()` (CUSBFunction) →
  `GetDeviceDescriptor()` → `idVendor` / `idProduct`.
- The logical button set matches `enum class PadButton { A,B,X,Y,L,R,Start,Select }`
  exactly (8 in that order), and `pad_bit(PadButton)` already maps a `PadButton` to
  its `GP_*` bit — reused by the decode.

## Data Model & File Format

One controller's calibration:

```cpp
struct ControllerCal {
    uint16_t vid;        // USB idVendor
    uint16_t pid;        // USB idProduct
    uint8_t  bit[8];     // raw HID bit index per PadButton (A,B,X,Y,L,R,Start,Select);
                         // 255 = unmapped (button skipped / not present)
};
```

`SD:/controllers.txt`, one calibration per line, `#` comments allowed:

```
# vid:pid=A,B,X,Y,L,R,Start,Select  (raw HID bit indices, 255=unmapped)
2dc8:6101=0,1,3,4,6,7,10,11
```

- `vid`/`pid` are 4-digit hex; the 8 bit indices are decimal `0..31` or `255`.
- Bit-index order is `PadButton` order (A,B,X,Y,L,R,Start,Select).

## Pure Module `src/input/controller_map.{h,cpp}` (host-tested)

No Circle dependency (includes `joypad_map.h` for `pad_bit`/`GP_*`/`PadButton`,
like `test_joypad`/`test_hotkey`):

```cpp
struct ControllerCal { uint16_t vid; uint16_t pid; uint8_t bit[8]; };

// Parse one "vid:pid=b0,...,b7" line into out. Returns false on bad hex,
// wrong field count, or an out-of-range bit (>31 and !=255). '#'/blank → false.
bool controller_parse_line(const char *line, ControllerCal *out);

// Render a ControllerCal to "vid:pid=b0,...,b7" (NUL-terminated, bounded).
void controller_serialize_line(const ControllerCal &c, char *out, size_t out_size);

// Map a controller's raw HID button word to the face/shoulder GP_* mask, using
// the calibration: for i in 0..7, if bit[i]!=255 and (raw>>bit[i])&1, set
// pad_bit((PadButton)i). (D-pad is added separately by the caller.)
unsigned controller_decode(unsigned raw_buttons, const ControllerCal &cal);

// Linear search by vid/pid in an array; returns the match or nullptr.
const ControllerCal *controller_find(const ControllerCal *arr, unsigned count,
                                     uint16_t vid, uint16_t pid);
```

## Runtime Decode (`src/input/gamepad.{h,cpp}`)

Today `decode_buttons()` does `unsigned b = pState->buttons;` (8BitDo identity).
Changes, all using the existing per-slot static-cache pattern:

- New caches: `static volatile unsigned s_raw[MAX_PADS]` (raw `buttons`),
  `static const ControllerCal *s_cal[MAX_PADS]` (active calibration, null =
  fallback), and `s_vid[MAX_PADS]` / `s_pid[MAX_PADS]`.
- The per-slot handlers set `s_raw[slot] = pState->buttons` and compute the face
  bits via `decode_buttons(slot, pState)`:
  ```
  face = s_cal[slot] ? controller_decode(s_raw[slot], *s_cal[slot])
                     : (unsigned) pState->buttons;   // unchanged 8BitDo path
  // + the existing hat + analog-axis D-pad synthesis (unchanged)
  ```
- In `Poll()`'s `pad_reconcile` **Acquire** case, read the pad's VID/PID
  (`dev->GetDevice()->GetDeviceDescriptor()`), cache `s_vid/s_pid[i]`, and set
  `s_cal[i] = m_pCal ? controller_find(m_pCal, m_nCal, vid, pid) : 0`. On **Clear**,
  null `s_cal[i]`.
- New `Gamepad` members: a pointer+count to the loaded calibration array
  (`const ControllerCal *m_pCal; unsigned m_nCal;`, set by the kernel after load)
  and accessors `unsigned RawButtons(port)`, `unsigned VendorId(port)`,
  `unsigned ProductId(port)` for the calibration screen, plus
  `void SetCalibrations(const ControllerCal*, unsigned)` and a way to re-run the
  lookup for a port after a new calibration is saved (`ReacquireCal(port)`).

Everything downstream (`joypad_state`, `ButtonMap`, menus) is unchanged — this is a
pure new layer between raw HID and the `GP_*` normalization. An uncalibrated/8BitDo
pad behaves exactly as today.

## ControllerStore (`src/input/controller_store.{h,cpp}`, Circle)

Owns the in-memory calibration array and the file I/O (thin; parsing/serializing is
the pure module):

- `bool Load(Storage*)` — read `SD:/controllers.txt`, `controller_parse_line` each
  line into a fixed array (e.g. `MAX_CONTROLLERS = 16`); ignore bad/`#` lines.
- `const ControllerCal *Data() const; unsigned Count() const;` — handed to
  `Gamepad::SetCalibrations`.
- `void Upsert(const ControllerCal&)` — replace the entry with the same vid/pid, or
  append (bounded by `MAX_CONTROLLERS`).
- `bool Save(Storage*)` — serialize all entries back to `SD:/controllers.txt`.

## Calibration Screen (`src/menu/calibration_screen.{h,cpp}`)

A TextCanvas screen, same constructor pattern as `ControlsScreen`
(`TextCanvas*, Gamepad*, CUSBHCIDevice*, ControllerStore*, Storage*`), reached from
a Settings "Calibrate Controller..." row (opened with **Start**).

`Run()`:
1. If `!m_pGamepad->IsPresent(0)` → draw "No controller on Port 1", wait for the
   D-pad or a brief delay, return.
2. Read `VendorId(0)`/`ProductId(0)` into a working `ControllerCal` (bits start
   255). Show `Calibrating  VID:PID` at the top.
3. For `i` in 0..7, prompt `Press <name[i]>   (D-pad Down: skip, Left: cancel)`:
   - poll; `raw = RawButtons(0)`; `pressed = raw & ~prevRaw`; if `pressed`, record
     the lowest set bit index into `cal.bit[i]` and advance.
   - `nav = Buttons(0)`: a new `GP_DOWN` → `cal.bit[i]=255`, advance; a new
     `GP_LEFT` → return without saving (cancel).
4. After the 8th: `m_pStore->Upsert(cal)`; `m_pStore->Save(m_pStorage)`;
   `m_pGamepad->SetCalibrations(m_pStore->Data(), m_pStore->Count())` then
   `ReacquireCal(0)` so it applies immediately; show "Saved", return.

Capture reads the **raw** bits (mapping-independent); the **D-pad** (generic
hat/axis) is the only navigation needed pre-calibration, resolving the
chicken-and-egg.

## Settings / Kernel Wiring

- `SettingsScreen` gains a `CalibrationScreen *` (forward-declared like
  `ControlsScreen`) and a `Calibrate Controller...` action row opening it on Start.
- `kernel.cpp`: own a `ControllerStore` and a `CalibrationScreen`; after SD mount,
  `m_ControllerStore.Load(&m_Storage)` and
  `m_Gamepad.SetCalibrations(m_ControllerStore.Data(), m_ControllerStore.Count())`
  before the play loop, so `Poll()` picks the right calibration on acquire.

## Error Handling

- Missing/empty/corrupt `controllers.txt` → zero calibrations loaded; every pad uses
  the 8BitDo fallback (today's behavior). Bad lines are skipped individually.
- A controller whose VID/PID isn't in the store → fallback mapping (the screen lets
  the user fix it by calibrating).
- A button skipped during capture (`255`) simply never sets its `GP_*` bit.
- `MAX_CONTROLLERS` reached on `Upsert` of a *new* vid/pid → keep existing, drop the
  new one (rare; documented). Re-calibrating an existing vid/pid always replaces.
- Save failure (SD write) → the in-memory calibration still applies for the session;
  logged, not surfaced.

## Testing

**Host unit tests** (`test/test_controller_map.cpp`, added to `test/Makefile`,
linking `controller_map.cpp` + `joypad_map.cpp` with `-I$(LIBRETRO_INC)`):
- `controller_parse_line`: a valid line → right vid/pid/bits; reject bad hex, wrong
  field count, out-of-range bit, `#`/blank.
- round-trip: `controller_serialize_line` then `controller_parse_line` preserves a
  `ControllerCal`.
- `controller_decode`: raw word with specific bits set → exactly the expected
  `GP_*` mask; `255` entries contribute nothing; a calibration that remaps
  (e.g. A↔B swapped relative to identity) produces the swapped `GP_*` bits.
- `controller_find`: hit and miss.

(The Gamepad acquire/VID-read, the ControllerStore file I/O, and the calibration
screen are Circle/hardware — verified on hardware, like the rest of the input/menu
code. Build verification is the repo-root `make`.)

**Hardware verification** (new checklist item):
- With a non-8BitDo pad as P1, buttons are wrong/dead → Settings → Calibrate
  Controller → press each button → after saving, all 8 buttons work in-game.
- `SD:/controllers.txt` contains the `vid:pid=…` line; survives reboot and applies
  on next boot without re-calibrating.
- D-pad Down skips a button (leaves it dead); D-pad Left cancels without saving.
- An 8BitDo (or any uncalibrated pad) still works out of the box (fallback).

## Out of Scope

- D-pad calibration (stays on the hat/analog-axis path).
- Auto-prompting on an unknown controller (manual entry only).
- Analog stick / trigger / gyro mapping.
- Per-physical-port calibration (keyed by model, not port).
- A multi-pad selector in the calibration screen (calibrates port 1; plug the
  target pad as P1).

## Cross-references

- Raw HID decode + the 8BitDo assumption — `src/input/gamepad.cpp`,
  `src/input/joypad_map.h`.
- Circle gamepad state + descriptor — `libs/circle/include/circle/usb/usbgamepad.h`,
  `usbdevice.h`.
- Two-controller input layer — [[project-two-controllers]].
- FSD §4.9 / input — `Documents/Bare-Metal-Sega-Genesis-FSD.md`.
</content>
