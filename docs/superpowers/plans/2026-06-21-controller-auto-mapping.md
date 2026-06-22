# Controller Auto-Mapping (Calibration) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the user calibrate any USB gamepad (per VID:PID) via a press-each-button screen, remapping its raw HID bits into our `GP_*` slots, with the 8BitDo identity as the uncalibrated fallback.

**Architecture:** A pure host-tested `controller_map` (parse/serialize/decode/find); a `ControllerStore` (SD:/controllers.txt I/O); `Gamepad` gains a raw-bit cache + per-port active calibration chosen by VID/PID on acquire; a `CalibrationScreen` captures raw presses (D-pad = skip/cancel); Settings + kernel wire it together.

**Tech Stack:** C++ (bare-metal, Circle), GNU make. Host tests in `test/`. Kernel cross-compiles to `kernel7.img` via repo-root `make`.

## Global Constraints

- Pure `src/input/controller_map.cpp` has no Circle dependency (it includes `joypad_map.h` for `pad_bit`/`GP_*`/`PadButton`; its host test links `joypad_map.cpp` with `-I$(LIBRETRO_INC)`, like `test_hotkey`).
- `ControllerCal`: `uint16_t vid, pid; uint8_t bit[8];` — `bit[i]` = raw HID bit index for `PadButton` i (A,B,X,Y,L,R,Start,Select); `255` = unmapped.
- File `SD:/controllers.txt`: lines `vid:pid=b0,...,b7`, vid/pid 4-hex, bits decimal 0..31 or 255; `#`/blank ignored.
- An uncalibrated pad must behave **exactly** as today (raw `buttons` passthrough). Only a calibrated pad's bits get remapped.
- D-pad stays on the existing hat/analog-axis synthesis (not calibrated).
- `MAX_CONTROLLERS = 16`.

---

### Task 1: Pure `controller_map` module (host-tested)

**Files:**
- Create: `src/input/controller_map.h`
- Create: `src/input/controller_map.cpp`
- Test: `test/test_controller_map.cpp`
- Modify: `test/Makefile`

**Interfaces:**
- Produces:
  - `struct ControllerCal { uint16_t vid; uint16_t pid; uint8_t bit[8]; };`
  - `bool controller_parse_line(const char *line, ControllerCal *out);`
  - `void controller_serialize_line(const ControllerCal &c, char *out, size_t out_size);`
  - `unsigned controller_decode(unsigned raw_buttons, const ControllerCal &cal);`
  - `const ControllerCal *controller_find(const ControllerCal *arr, unsigned count, uint16_t vid, uint16_t pid);`

- [ ] **Step 1: Create the header `src/input/controller_map.h`**

```cpp
//
// src/input/controller_map.h
//
// Bare Metal Sega Genesis
// Per-controller (VID:PID) calibration: map a pad's raw HID button bits onto our
// logical GP_* slots. Pure (no Circle dep) so it is host-testable.
//

#ifndef _input_controller_map_h
#define _input_controller_map_h

#include <stdint.h>
#include <stddef.h>

// One controller model's calibration. bit[i] is the raw HID button-bit index for
// PadButton i (A,B,X,Y,L,R,Start,Select); 255 = unmapped (button skipped/absent).
struct ControllerCal { uint16_t vid; uint16_t pid; uint8_t bit[8]; };

// Parse "vid:pid=b0,b1,...,b7" (vid/pid 4-hex, bits decimal 0..31 or 255) into
// out. Returns false on bad hex, wrong field count, an out-of-range bit, or a
// '#'/blank line.
bool controller_parse_line(const char *line, ControllerCal *out);

// Render c to "vid:pid=b0,...,b7" (NUL-terminated, truncated to out_size).
void controller_serialize_line(const ControllerCal &c, char *out, size_t out_size);

// Map raw_buttons -> face/shoulder GP_* mask: for i in 0..7, if bit[i]!=255 and
// (raw>>bit[i])&1, OR in pad_bit((PadButton)i). (D-pad handled by the caller.)
unsigned controller_decode(unsigned raw_buttons, const ControllerCal &cal);

// Find a calibration by vid/pid in arr[0..count); nullptr if absent.
const ControllerCal *controller_find(const ControllerCal *arr, unsigned count,
                                     uint16_t vid, uint16_t pid);

#endif
```

- [ ] **Step 2: Write the failing test `test/test_controller_map.cpp`**

```cpp
#include "../src/input/controller_map.h"
#include "../src/input/joypad_map.h"   // GP_*, pad_bit, PadButton
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    // Parse a valid line.
    ControllerCal c;
    assert(controller_parse_line("2dc8:6101=0,1,3,4,6,7,10,11", &c));
    assert(c.vid == 0x2dc8 && c.pid == 0x6101);
    assert(c.bit[0] == 0 && c.bit[2] == 3 && c.bit[7] == 11);

    // 255 (unmapped) allowed.
    ControllerCal u;
    assert(controller_parse_line("1:2=0,255,2,3,4,5,6,7", &u));
    assert(u.bit[1] == 255);

    // Rejections: bad hex, wrong field count, out-of-range bit, comment/blank.
    assert(!controller_parse_line("zz:6101=0,1,2,3,4,5,6,7", &c));
    assert(!controller_parse_line("2dc8:6101=0,1,2,3,4,5,6", &c));    // 7 bits
    assert(!controller_parse_line("2dc8:6101=0,1,2,3,4,5,6,99", &c)); // 99 > 31, !=255
    assert(!controller_parse_line("# comment", &c));
    assert(!controller_parse_line("", &c));

    // Round-trip.
    ControllerCal in = { 0x045e, 0x028e, { 0,1,2,3,4,5,6,7 } };
    char line[64];
    controller_serialize_line(in, line, sizeof line);
    ControllerCal out;
    assert(controller_parse_line(line, &out));
    assert(out.vid == in.vid && out.pid == in.pid);
    for (int i = 0; i < 8; i++) assert(out.bit[i] == in.bit[i]);

    // Decode: identity-ish mapping sets the matching GP_* bits.
    ControllerCal idc = { 1, 1, { 0,1,2,3,4,5,9,8 } };  // A..Z bits, Start=bit9, Select=bit8
    unsigned raw = (1u << 0) | (1u << 1);               // A and B pressed
    unsigned m = controller_decode(raw, idc);
    assert(m == (pad_bit(PadButton::A) | pad_bit(PadButton::B)));

    // Decode: a remap (raw bit 1 -> A) and a 255 (unmapped) contributes nothing.
    ControllerCal swp = { 1, 1, { 1,0,2,3,4,5,9,8 } };  // A<-bit1, B<-bit0
    assert(controller_decode((1u << 1), swp) == pad_bit(PadButton::A));
    ControllerCal none = { 1, 1, { 255,255,255,255,255,255,255,255 } };
    assert(controller_decode(0xFFFFFFFF, none) == 0);

    // Find.
    ControllerCal arr[2] = { { 0x11,0x22,{0} }, { 0x33,0x44,{0} } };
    assert(controller_find(arr, 2, 0x33, 0x44) == &arr[1]);
    assert(controller_find(arr, 2, 0x99, 0x99) == 0);

    printf("All controller_map tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the `test_controller_map` target to `test/Makefile`**

Add `test_controller_map` to the `run:` dependency list and its `./test_controller_map` line, add the build rule (it needs the libretro include like `test_joypad`), and add it to the `clean:` `rm -f` list:

```make
test_controller_map: test_controller_map.cpp ../src/input/controller_map.cpp ../src/input/controller_map.h ../src/input/joypad_map.cpp
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_controller_map.cpp ../src/input/controller_map.cpp ../src/input/joypad_map.cpp
```

- [ ] **Step 4: Run the test to verify it fails (does not link)**

Run: `cd test && make test_controller_map`
Expected: FAIL — `undefined reference to controller_parse_line` etc.

- [ ] **Step 5: Implement `src/input/controller_map.cpp`**

```cpp
//
// src/input/controller_map.cpp
//
// Bare Metal Sega Genesis
// See controller_map.h.
//

#include "controller_map.h"
#include "joypad_map.h"   // pad_bit, PadButton

// Parse up to 4 hex digits at *p (advancing p); returns the value, sets ok.
static unsigned parse_hex(const char *&p, bool &ok)
{
    unsigned v = 0;
    int n = 0;
    while (*p)
    {
        char ch = *p;
        unsigned d;
        if      (ch >= '0' && ch <= '9') d = (unsigned)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') d = (unsigned)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') d = (unsigned)(ch - 'A' + 10);
        else break;
        v = v * 16 + d; n++; p++;
    }
    ok = (n > 0);
    return v;
}

// Parse a decimal at *p (advancing p); returns the value, sets ok.
static unsigned parse_dec(const char *&p, bool &ok)
{
    unsigned v = 0;
    int n = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (unsigned)(*p - '0'); n++; p++; }
    ok = (n > 0);
    return v;
}

bool controller_parse_line(const char *line, ControllerCal *out)
{
    if (line == 0) return false;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '#' || *line == '\0') return false;

    const char *p = line;
    bool ok = false;
    unsigned vid = parse_hex(p, ok);  if (!ok || *p != ':') return false;
    p++;
    unsigned pid = parse_hex(p, ok);  if (!ok || *p != '=') return false;
    p++;

    uint8_t bits[8];
    for (int i = 0; i < 8; i++)
    {
        unsigned b = parse_dec(p, ok);
        if (!ok) return false;
        if (b != 255 && b > 31) return false;
        bits[i] = (uint8_t) b;
        if (i < 7) { if (*p != ',') return false; p++; }
    }
    // trailing chars after the 8th value (other than whitespace) are rejected
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '\0') return false;

    out->vid = (uint16_t) vid;
    out->pid = (uint16_t) pid;
    for (int i = 0; i < 8; i++) out->bit[i] = bits[i];
    return true;
}

// Append a 4-digit lowercase hex of v to out[*len], bounded.
static void app_hex4(char *out, size_t *len, size_t cap, unsigned v)
{
    static const char hx[] = "0123456789abcdef";
    for (int shift = 12; shift >= 0; shift -= 4)
        if (*len + 1 < cap) out[(*len)++] = hx[(v >> shift) & 0xF];
    out[*len] = '\0';
}

// Append decimal v to out[*len], bounded.
static void app_dec(char *out, size_t *len, size_t cap, unsigned v)
{
    char tmp[4]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0 && *len + 1 < cap) out[(*len)++] = tmp[--n];
    out[*len] = '\0';
}

static void app_ch(char *out, size_t *len, size_t cap, char c)
{
    if (*len + 1 < cap) out[(*len)++] = c;
    out[*len] = '\0';
}

void controller_serialize_line(const ControllerCal &c, char *out, size_t out_size)
{
    if (out == 0 || out_size == 0) return;
    size_t len = 0; out[0] = '\0';
    app_hex4(out, &len, out_size, c.vid);
    app_ch(out, &len, out_size, ':');
    app_hex4(out, &len, out_size, c.pid);
    app_ch(out, &len, out_size, '=');
    for (int i = 0; i < 8; i++)
    {
        if (i) app_ch(out, &len, out_size, ',');
        app_dec(out, &len, out_size, c.bit[i]);
    }
}

unsigned controller_decode(unsigned raw_buttons, const ControllerCal &cal)
{
    unsigned m = 0;
    for (int i = 0; i < 8; i++)
    {
        uint8_t b = cal.bit[i];
        if (b != 255 && ((raw_buttons >> b) & 1u))
            m |= pad_bit((PadButton) i);
    }
    return m;
}

const ControllerCal *controller_find(const ControllerCal *arr, unsigned count,
                                     uint16_t vid, uint16_t pid)
{
    for (unsigned i = 0; i < count; i++)
        if (arr[i].vid == vid && arr[i].pid == pid) return &arr[i];
    return 0;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd test && make test_controller_map && ./test_controller_map`
Expected: PASS — `All controller_map tests passed`.

- [ ] **Step 7: Full host suite (no regressions)**

Run: `cd test && make && ./test_controller_map && ./test_joypad && ./test_settings`
Expected: each prints its pass line.

- [ ] **Step 8: Commit**

```bash
git add src/input/controller_map.h src/input/controller_map.cpp test/test_controller_map.cpp test/Makefile
git commit -m "Input: pure controller_map (VID:PID calibration parse/decode) + host test

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Gamepad runtime decode (raw cache + per-port calibration)

**Files:**
- Modify: `src/input/gamepad.h` (include, members, accessor declarations)
- Modify: `src/input/gamepad.cpp` (static caches, calibration-aware `decode_buttons`, VID/PID read in `Poll`, accessor defs)

**Interfaces:**
- Consumes: `ControllerCal`, `controller_decode`, `controller_find` (Task 1).
- Produces (new `Gamepad` API):
  - `unsigned RawButtons(unsigned port) const;`
  - `unsigned VendorId(unsigned port) const;`
  - `unsigned ProductId(unsigned port) const;`
  - `void SetCalibrations(const ControllerCal *arr, unsigned count);`
  - `void ReacquireCal(unsigned port);`

- [ ] **Step 1: Extend `gamepad.h`**

Add the include after the existing includes:

```cpp
#include "controller_map.h"
```

Add the public methods after `MenuButtons`:

```cpp
    // Raw (pre-calibration) HID button word for a port — for the calibration screen.
    unsigned RawButtons(unsigned port = 0) const;
    // USB identifiers of the pad on a port (0 if absent).
    unsigned VendorId(unsigned port = 0) const;
    unsigned ProductId(unsigned port = 0) const;

    // Point the decoder at the loaded per-controller calibrations.
    void SetCalibrations(const ControllerCal *arr, unsigned count);
    // Re-pick a port's active calibration from its cached VID/PID (after a save).
    void ReacquireCal(unsigned port);
```

Add the members after `m_pDevice`:

```cpp
    const ControllerCal *m_pCal;
    unsigned             m_nCal;
```

- [ ] **Step 2: Add the static caches in `gamepad.cpp`**

After the existing `static volatile unsigned s_buttons[Gamepad::MAX_PADS] = { 0, 0 };`, add:

```cpp
// Raw HID button word per port (pre-calibration), and the active per-port
// calibration + cached USB ids. Static because the report handlers take no
// userdata (same reason as s_buttons).
static volatile unsigned       s_raw[Gamepad::MAX_PADS] = { 0, 0 };
static const ControllerCal    *s_cal[Gamepad::MAX_PADS] = { 0, 0 };
static unsigned                s_vid[Gamepad::MAX_PADS] = { 0, 0 };
static unsigned                s_pid[Gamepad::MAX_PADS] = { 0, 0 };
```

- [ ] **Step 3: Make `decode_buttons` calibration-aware**

Change the signature and the face-bit source. Replace:

```cpp
static unsigned decode_buttons(const TGamePadState *pState)
{
    unsigned b = (unsigned) pState->buttons;   // raw HID digital buttons
```

with:

```cpp
static unsigned decode_buttons(unsigned slot, const TGamePadState *pState)
{
    unsigned raw = (unsigned) pState->buttons;   // raw HID digital buttons
    s_raw[slot] = raw;
    // Calibrated pad: remap raw bits to GP_* slots. Uncalibrated: passthrough
    // (today's 8BitDo identity behavior, unchanged).
    unsigned b = s_cal[slot] ? controller_decode(raw, *s_cal[slot]) : raw;
```

(The rest of the function — the hat `switch` and the analog-axis block that `|=`
direction bits into `b` — is unchanged.)

- [ ] **Step 4: Update the two handlers to pass their slot**

Replace:

```cpp
static void Handler0(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[0] = decode_buttons(pState);
}

static void Handler1(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[1] = decode_buttons(pState);
}
```

with:

```cpp
static void Handler0(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[0] = decode_buttons(0, pState);
}

static void Handler1(unsigned /*idx*/, const TGamePadState *pState)
{
    s_buttons[1] = decode_buttons(1, pState);
}
```

- [ ] **Step 5: Initialize the new members + add the device includes**

Add includes near the top of `gamepad.cpp` (after the existing includes):

```cpp
#include <circle/usb/usbdevice.h>
#include <circle/usb/usb.h>
```

In the constructor, initialize the calibration pointer/count (extend the ctor body):

```cpp
Gamepad::Gamepad(CDeviceNameService *pNameService)
:   m_pNameService(pNameService), m_pCal(0), m_nCal(0)
{
    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        m_pDevice[i] = 0;
    }
}
```

- [ ] **Step 6: Read VID/PID and pick the calibration on acquire/clear**

In `Poll()`, update the `Clear` and `Acquire` cases:

```cpp
        case PadAction::Clear:           // unplugged: stop ghost inputs, free slot
            s_buttons[i] = 0;
            s_raw[i] = 0;
            s_cal[i] = 0;
            s_vid[i] = s_pid[i] = 0;
            m_pDevice[i] = 0;
            break;

        case PadAction::Acquire:         // first plug / re-plug / swap
        {
            m_pDevice[i] = dev;
            s_buttons[i] = 0;
            s_raw[i] = 0;

            u16 vid = 0, pid = 0;
            CUSBDevice *udev = dev->GetDevice();
            if (udev != 0)
            {
                const TUSBDeviceDescriptor *d = udev->GetDeviceDescriptor();
                if (d != 0) { vid = d->idVendor; pid = d->idProduct; }
            }
            s_vid[i] = vid;
            s_pid[i] = pid;
            s_cal[i] = (m_pCal && m_nCal)
                       ? controller_find(m_pCal, m_nCal, vid, pid) : 0;

            dev->RegisterStatusHandler(handlers[i]);
            break;
        }
```

- [ ] **Step 7: Implement the new accessors at the end of `gamepad.cpp`**

```cpp
unsigned Gamepad::RawButtons(unsigned port) const
{
    return port < MAX_PADS ? s_raw[port] : 0;
}

unsigned Gamepad::VendorId(unsigned port) const
{
    return port < MAX_PADS ? s_vid[port] : 0;
}

unsigned Gamepad::ProductId(unsigned port) const
{
    return port < MAX_PADS ? s_pid[port] : 0;
}

void Gamepad::SetCalibrations(const ControllerCal *arr, unsigned count)
{
    m_pCal = arr;
    m_nCal = count;
}

void Gamepad::ReacquireCal(unsigned port)
{
    if (port < MAX_PADS)
        s_cal[port] = (m_pCal && m_nCal)
                      ? controller_find(m_pCal, m_nCal, s_vid[port], s_pid[port]) : 0;
}
```

- [ ] **Step 8: Add `controller_map.o` to the kernel `Makefile` OBJS**

After `src/input/pad_reconcile.o \` in the `OBJS =` list, add:

```make
       src/input/controller_map.o \
```

- [ ] **Step 9: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion; `kernel7.img` produced. With no calibrations set
(`m_pCal == 0`), `decode_buttons` takes the passthrough path — behavior unchanged.

- [ ] **Step 10: Commit**

```bash
git add src/input/gamepad.h src/input/gamepad.cpp Makefile
git commit -m "Input: Gamepad raw cache + per-port VID/PID calibration in decode_buttons

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: ControllerStore (SD:/controllers.txt I/O)

**Files:**
- Create: `src/input/controller_store.h`
- Create: `src/input/controller_store.cpp`
- Modify: `Makefile` (OBJS)

**Interfaces:**
- Consumes: `ControllerCal`, `controller_parse_line`, `controller_serialize_line` (Task 1); `Storage::ReadFile/WriteFile`.
- Produces: `ControllerStore` with `Load/Save/Data/Count/Upsert`.

- [ ] **Step 1: Create `src/input/controller_store.h`**

```cpp
//
// src/input/controller_store.h
//
// Bare Metal Sega Genesis
// Loads/saves per-controller calibrations from SD:/controllers.txt and holds them
// for the Gamepad decoder. Parsing/serialization is the pure controller_map.
//

#ifndef _input_controller_store_h
#define _input_controller_store_h

#include "controller_map.h"
#include "../storage/storage.h"

class ControllerStore
{
public:
    static const unsigned MAX_CONTROLLERS = 16;

    ControllerStore(void);

    // Read SD:/controllers.txt (missing/empty -> 0 entries). Returns true if the
    // file was read.
    bool Load(Storage *pStorage);
    // Write all entries back to SD:/controllers.txt.
    bool Save(Storage *pStorage);

    const ControllerCal *Data(void) const { return m_cal; }
    unsigned             Count(void) const { return m_count; }

    // Replace the entry with the same vid/pid, or append (bounded by
    // MAX_CONTROLLERS — a new vid/pid past the limit is dropped).
    void Upsert(const ControllerCal &c);

private:
    ControllerCal m_cal[MAX_CONTROLLERS];
    unsigned      m_count;
};

#endif
```

- [ ] **Step 2: Create `src/input/controller_store.cpp`**

```cpp
//
// src/input/controller_store.cpp
//
// Bare Metal Sega Genesis
// See controller_store.h.
//

#include "controller_store.h"

static const char *PATH = "SD:/controllers.txt";

ControllerStore::ControllerStore(void)
:   m_count(0)
{
}

bool ControllerStore::Load(Storage *pStorage)
{
    m_count = 0;
    if (pStorage == 0 || !pStorage->Exists(PATH)) return false;

    u8    *buf  = 0;
    size_t size = 0;
    if (!pStorage->ReadFile(PATH, &buf, &size)) return false;

    // Walk lines in place (the buffer is our own copy).
    char *text = (char *) buf;
    size_t i = 0;
    while (i < size && m_count < MAX_CONTROLLERS)
    {
        size_t start = i;
        while (i < size && text[i] != '\n' && text[i] != '\r') i++;
        char saved = text[i < size ? i : start];
        if (i < size) text[i] = '\0';            // terminate this line
        ControllerCal c;
        if (controller_parse_line(text + start, &c)) m_cal[m_count++] = c;
        if (i < size) text[i] = saved;           // restore (harmless)
        while (i < size && (text[i] == '\n' || text[i] == '\r')) i++;
    }

    delete[] buf;
    return true;
}

bool ControllerStore::Save(Storage *pStorage)
{
    if (pStorage == 0) return false;

    char out[64 + MAX_CONTROLLERS * 48];
    size_t len = 0;
    const char *hdr = "# vid:pid=A,B,X,Y,L,R,Start,Select (raw HID bits, 255=unmapped)\n";
    for (const char *p = hdr; *p && len + 1 < sizeof out; p++) out[len++] = *p;

    for (unsigned k = 0; k < m_count; k++)
    {
        char line[48];
        controller_serialize_line(m_cal[k], line, sizeof line);
        for (const char *p = line; *p && len + 1 < sizeof out; p++) out[len++] = *p;
        if (len + 1 < sizeof out) out[len++] = '\n';
    }

    return pStorage->WriteFile(PATH, (const u8 *) out, len);
}

void ControllerStore::Upsert(const ControllerCal &c)
{
    for (unsigned k = 0; k < m_count; k++)
        if (m_cal[k].vid == c.vid && m_cal[k].pid == c.pid) { m_cal[k] = c; return; }
    if (m_count < MAX_CONTROLLERS) m_cal[m_count++] = c;
}
```

- [ ] **Step 3: Add `controller_store.o` to the kernel `Makefile` OBJS**

After the `src/input/controller_map.o \` line, add:

```make
       src/input/controller_store.o \
```

- [ ] **Step 4: Build the kernel**

Run: `make` (from the repo root)
Expected: compiles `controller_store.o`, links, `kernel7.img` produced, no errors.

- [ ] **Step 5: Commit**

```bash
git add src/input/controller_store.h src/input/controller_store.cpp Makefile
git commit -m "Input: ControllerStore (SD:/controllers.txt load/save/upsert)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: CalibrationScreen (capture UI)

**Files:**
- Create: `src/menu/calibration_screen.h`
- Create: `src/menu/calibration_screen.cpp`
- Modify: `Makefile` (OBJS)

**Interfaces:**
- Consumes: `Gamepad` (`RawButtons`/`VendorId`/`ProductId`/`Buttons`/`IsPresent`/`SetCalibrations`/`ReacquireCal`), `ControllerStore`, `Storage`, `GP_DOWN`/`GP_LEFT` (joypad_map).
- Produces: `CalibrationScreen` with `Run()`.

- [ ] **Step 1: Create `src/menu/calibration_screen.h`**

```cpp
//
// src/menu/calibration_screen.h
//
// Bare Metal Sega Genesis
// Press-each-button calibration for the port-1 controller; saves a per-VID/PID
// mapping to the ControllerStore. Reached from the Settings screen.
//

#ifndef _menu_calibration_screen_h
#define _menu_calibration_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../input/controller_store.h"
#include "../storage/storage.h"

class CalibrationScreen
{
public:
    CalibrationScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                      CUSBHCIDevice *pUSBHCI, ControllerStore *pStore,
                      Storage *pStorage);
    void Run(void);

private:
    void Prompt(unsigned vid, unsigned pid, int idx);
    void Message(const char *text);

    TextCanvas      *m_pCanvas;
    Gamepad         *m_pGamepad;
    CUSBHCIDevice   *m_pUSBHCI;
    ControllerStore *m_pStore;
    Storage         *m_pStorage;
};

#endif
```

- [ ] **Step 2: Create `src/menu/calibration_screen.cpp`**

```cpp
//
// src/menu/calibration_screen.cpp
//
// Bare Metal Sega Genesis
// See calibration_screen.h.
//

#include "calibration_screen.h"
#include "../input/joypad_map.h"   // GP_DOWN, GP_LEFT
#include <circle/timer.h>

static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;

static const char *const NAMES[8] = { "A", "B", "X", "Y", "L", "R", "Start", "Select" };

CalibrationScreen::CalibrationScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                                     CUSBHCIDevice *pUSBHCI, ControllerStore *pStore,
                                     Storage *pStorage)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pStore(pStore), m_pStorage(pStorage)
{
}

// Write 4-digit hex of v into out (>=5 bytes).
static void hex4(char *out, unsigned v)
{
    static const char hx[] = "0123456789abcdef";
    out[0] = hx[(v >> 12) & 0xF]; out[1] = hx[(v >> 8) & 0xF];
    out[2] = hx[(v >> 4) & 0xF];  out[3] = hx[v & 0xF]; out[4] = '\0';
}

void CalibrationScreen::Prompt(unsigned vid, unsigned pid, int idx)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    m_pCanvas->Clear(0x0000);
    m_pCanvas->FillRect(cw * 3, ch * 2, cw * 34, ch * 8, BOX);
    m_pCanvas->DrawText(cw * 4, ch * 3, "CALIBRATE CONTROLLER", WHITE, BOX);

    char vp[12]; char vh[5], ph[5];
    hex4(vh, vid); hex4(ph, pid);
    int k = 0;
    for (int j = 0; vh[j]; j++) vp[k++] = vh[j];
    vp[k++] = ':';
    for (int j = 0; ph[j]; j++) vp[k++] = ph[j];
    vp[k] = '\0';
    m_pCanvas->DrawText(cw * 4, ch * 4, vp, WHITE, BOX);

    char line[24] = "Press ";
    int n = 6;
    for (int j = 0; NAMES[idx][j] && n < 22; j++) line[n++] = NAMES[idx][j];
    line[n] = '\0';
    m_pCanvas->DrawText(cw * 4, ch * 6, line, WHITE, BOX);
    m_pCanvas->DrawText(cw * 4, ch * 8, "Down: skip   Left: cancel", WHITE, BOX);
}

void CalibrationScreen::Message(const char *text)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    m_pCanvas->Clear(0x0000);
    m_pCanvas->FillRect(cw * 3, ch * 2, cw * 34, ch * 3, BOX);
    m_pCanvas->DrawText(cw * 4, ch * 3, text, WHITE, BOX);
}

void CalibrationScreen::Run(void)
{
    if (!m_pGamepad->IsPresent(0))
    {
        Message("No controller on Port 1");
        CTimer::SimpleMsDelay(1500);
        return;
    }

    ControllerCal cal;
    cal.vid = (uint16_t) m_pGamepad->VendorId(0);
    cal.pid = (uint16_t) m_pGamepad->ProductId(0);
    for (int i = 0; i < 8; i++) cal.bit[i] = 255;

    int idx = 0;
    Prompt(cal.vid, cal.pid, idx);

    unsigned prevRaw = m_pGamepad->RawButtons(0);
    unsigned prevNav = m_pGamepad->Buttons(0);

    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();

        unsigned raw    = m_pGamepad->RawButtons(0);
        unsigned newRaw = raw & ~prevRaw;
        prevRaw = raw;

        unsigned nav    = m_pGamepad->Buttons(0);
        unsigned newNav = nav & ~prevNav;
        prevNav = nav;

        if (newNav & GP_LEFT) return;             // cancel, no save

        bool advanced = false;
        if (newNav & GP_DOWN)                     // skip this button
        {
            cal.bit[idx] = 255;
            advanced = true;
        }
        else if (newRaw)                          // captured a raw bit
        {
            unsigned bit = 0;
            while (bit < 31 && !((newRaw >> bit) & 1u)) bit++;
            cal.bit[idx] = (uint8_t) bit;
            advanced = true;
        }

        if (advanced)
        {
            idx++;
            if (idx >= 8) break;
            Prompt(cal.vid, cal.pid, idx);
            CTimer::SimpleMsDelay(150);            // debounce between buttons
            prevRaw = m_pGamepad->RawButtons(0);
            prevNav = m_pGamepad->Buttons(0);
            continue;
        }

        CTimer::SimpleMsDelay(16);
    }

    m_pStore->Upsert(cal);
    m_pStore->Save(m_pStorage);
    m_pGamepad->SetCalibrations(m_pStore->Data(), m_pStore->Count());
    m_pGamepad->ReacquireCal(0);
    m_pGamepad->ReacquireCal(1);

    Message("Saved");
    CTimer::SimpleMsDelay(800);
}
```

- [ ] **Step 3: Add `calibration_screen.o` to the kernel `Makefile` OBJS**

After the `src/menu/...` group (e.g. after `src/menu/hotkey_screen.o \`), add:

```make
       src/menu/calibration_screen.o \
```

(If the exact menu `.o` lines differ, place it alongside the other `src/menu/*.o` entries.)

- [ ] **Step 4: Build the kernel**

Run: `make` (from the repo root)
Expected: compiles `calibration_screen.o`, links, `kernel7.img` produced, no errors.

- [ ] **Step 5: Commit**

```bash
git add src/menu/calibration_screen.h src/menu/calibration_screen.cpp Makefile
git commit -m "Menu: CalibrationScreen — press-each-button capture, D-pad skip/cancel

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: Settings row + kernel wiring + hardware checklist

**Files:**
- Modify: `src/menu/settings_screen.h` (forward-declare `CalibrationScreen`, ctor param, member)
- Modify: `src/menu/settings_screen.cpp` (ctor, NUM_ROWS, labels, GP_START action)
- Modify: `src/kernel.h` (members: `ControllerStore`, `CalibrationScreen`)
- Modify: `src/kernel.cpp` (includes, ctor init, load store + SetCalibrations, pass screen to SettingsScreen)
- Modify: `docs/hardware-verification-checklist-2026-06-20.md`

**Interfaces:**
- Consumes: `CalibrationScreen` (Task 4), `ControllerStore` (Task 3), `Gamepad::SetCalibrations` (Task 2).

- [ ] **Step 1: Thread `CalibrationScreen` into `SettingsScreen` (header)**

In `src/menu/settings_screen.h`: add a forward declaration `class CalibrationScreen;`
near the other forward declarations (e.g. `class VideoModeScreen;`), add a
`CalibrationScreen *pCalibration` parameter to the constructor (last parameter), and
a `CalibrationScreen *m_pCalibration;` member (next to `m_pHotkey`).

- [ ] **Step 2: Settings screen ctor + open the screen (cpp)**

In `src/menu/settings_screen.cpp`:
- include `#include "calibration_screen.h"` near the other screen includes.
- add `CalibrationScreen *pCalibration` to the constructor signature and
  `m_pCalibration(pCalibration)` to the initializer list.
- bump `#define NUM_ROWS 15` to `#define NUM_ROWS 16`.
- in `Render`, add `"Calibrate..."` to the end of the `labels` array (after
  `"Hotkeys..."`) and `""` to the end of the `values` array.
- in the `pressed & GP_START` block, add after the Hotkeys (`selected == 14`) case:

```cpp
            else if (selected == 15)                  // Calibrate Controller...
            {
                m_pCalibration->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
```

- [ ] **Step 3: Kernel members + includes (`kernel.h`)**

In `src/kernel.h`: add includes for `input/controller_store.h` and
`menu/calibration_screen.h` (near the other menu/input includes), and add members
`ControllerStore m_ControllerStore;` and `CalibrationScreen m_CalibrationScreen;`
(declared before `m_SettingsScreen`, since it is passed into it).

- [ ] **Step 4: Kernel ctor wiring (`kernel.cpp`)**

In the `CKernel` constructor initializer list:
- construct `m_CalibrationScreen` before `m_SettingsScreen`:
  `m_CalibrationScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_ControllerStore, &m_Storage),`
  (`m_ControllerStore` is default-constructed; if listed, `m_ControllerStore (),`).
- add `&m_CalibrationScreen` as the final argument to the `m_SettingsScreen (...)`
  constructor call.

- [ ] **Step 5: Load calibrations after SD mount (`kernel.cpp`)**

In `CKernel::Run`, after `splash_apply_override(...)` / `m_SettingsStore.Load(...)`
(before the play loop), add:

```cpp
	m_ControllerStore.Load (&m_Storage);
	m_Gamepad.SetCalibrations (m_ControllerStore.Data (), m_ControllerStore.Count ());
```

- [ ] **Step 6: Build the kernel**

Run: `make` (from the repo root)
Expected: builds to completion, `kernel7.img` produced, no errors. If the linker
reports a missing `CalibrationScreen`/`ControllerStore` symbol, confirm their `.o`
entries are in `OBJS` (Tasks 3 & 4).

- [ ] **Step 7: Append the hardware-verify checklist section**

In `docs/hardware-verification-checklist-2026-06-20.md`, before `## Results summary`, add:

```markdown
## S. Controller calibration (auto-mapping)

- [ ] **S1 — Calibrate a non-8BitDo pad.** Plug a different USB pad as P1 (buttons
  wrong/dead). Settings → **Calibrate Controller...** → press each prompted button.
  **Expect:** after the 8th, "Saved"; in-game all 8 buttons now work correctly.
- [ ] **S2 — Persisted.** `SD:/controllers.txt` has a `vid:pid=...` line; power-cycle
  and the pad works without re-calibrating.
- [ ] **S3 — Skip / cancel.** D-pad **Down** skips a button (leaves it dead);
  D-pad **Left** cancels the screen with no change saved.
- [ ] **S4 — Fallback intact.** An 8BitDo (uncalibrated) still works out of the box.
```

Also add to the results-summary table:

```markdown
| S. Controller calib | | |
```

- [ ] **Step 8: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp src/kernel.h src/kernel.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Settings/Kernel: wire CalibrationScreen + load controller calibrations

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes for the implementer

- Host tests build natively in `test/`: `cd test && make`.
- The kernel build (`make` at repo root) is the only verification for the Circle
  pieces (gamepad, store, calibration screen, wiring), consistent with the project.
- The capture loop reads **raw** bits (`RawButtons`) so it works regardless of
  mapping; the **D-pad** (decoded via the generic hat/axis path) is the only
  navigation that works pre-calibration — that is deliberate.
- Don't touch `joypad_state`, `ButtonMap`, or `input_state_cb`; calibration is a new
  layer below the existing `GP_*` normalization.
</content>
