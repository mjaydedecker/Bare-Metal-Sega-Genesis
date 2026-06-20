# Video Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a settings subsystem (config file + in-emulation Settings screen) and use it to make video scaling mode (integer/stretch) and widescreen user-configurable.

**Architecture:** A new pure, host-tested `src/settings/` module holds a `Settings` struct with `parse`/`serialize` plus a `SettingsStore` that reads/writes `SD:/settings.txt` via the existing `Storage`. `Display` gains a stretch blit path selected by a scale mode; the libretro environment callback reads a `g_widescreen` global. A `SettingsScreen` (TextCanvas UI reached from the pause menu) edits the shared `Settings`, applies changes live where possible, and persists them. The kernel loads settings at boot and owns all the new objects.

**Tech Stack:** C++ (bare-metal, Circle framework), Genesis-Plus-GX-Wide libretro core, ChaN FatFS, host-side g++ unit tests.

## Global Constraints

- Pure logic files under `src/settings/settings.{h,cpp}` and `src/video/blit.{h,cpp}` MUST have **no Circle dependencies** (only `<stdint.h>`, `<stddef.h>`, `<string.h>`) so they compile with the host test toolchain.
- Settings file path is exactly `SD:/settings.txt` (a dedicated file — never the firmware's `config.txt`).
- Build with `-Wall -Wextra` (the project Makefile): constructor member-init order MUST match declaration order to avoid `-Wreorder` warnings.
- Every new `.cpp` under `src/` MUST be added to `OBJS` in the top-level `Makefile`; the new `src/settings/` directory MUST be added to `EXTRACLEAN`.
- New host test sources go in `test/`, are added to `test/Makefile`'s `run`/`clean` targets, and their built binaries added to `.gitignore`.
- Genesis-Plus-GX-Wide widescreen is controlled by the core variable `genesis_plus_gx_wide_h40_extra_columns`: `"0"` = native 320-wide (off), `"10"` = widescreen (on).
- Follow existing code style: 4-space indent, RGB565 color constants, `m_`-prefixed members, header guards `_dir_name_h`.

---

## File Structure

**New files:**
- `src/settings/settings.h` / `.cpp` — `Settings` struct, `ScaleMode` enum, pure `parse_settings`/`serialize_settings`.
- `src/settings/settings_store.h` / `.cpp` — `SettingsStore`: load/save `Settings` to `SD:/settings.txt` via `Storage`.
- `src/menu/settings_screen.h` / `.cpp` — `SettingsScreen` in-emulation UI.
- `test/test_settings.cpp` — host unit tests for parse/serialize.

**Modified files:**
- `src/video/blit.h` / `.cpp` — add `blit_rgb565_scaled` (non-integer nearest-neighbor scale into a rect).
- `src/video/display.h` / `.cpp` — add scale mode + stretch path.
- `src/libretro/environment.h` / `.cpp` — add `g_widescreen` / `g_variables_dirty`; use in `GET_VARIABLE` + handle `GET_VARIABLE_UPDATE`.
- `src/menu/pause_menu.h` / `.cpp` — enable the "Settings" entry, open `SettingsScreen`.
- `src/kernel.h` / `.cpp` — own `Settings` / `SettingsStore` / `SettingsScreen`; load at boot; wire into pause menu.
- `Makefile`, `test/Makefile`, `.gitignore`, `test/test_blit.cpp`.

---

## Task 1: Settings model (parse + serialize)

Pure, host-tested core of the settings subsystem.

**Files:**
- Create: `src/settings/settings.h`
- Create: `src/settings/settings.cpp`
- Create: `test/test_settings.cpp`
- Modify: `test/Makefile`
- Modify: `.gitignore`

**Interfaces:**
- Produces:
  - `enum class ScaleMode { Integer, Stretch };`
  - `struct Settings { ScaleMode scale_mode; bool widescreen; Settings(); };` (defaults: `Integer`, `false`)
  - `Settings parse_settings(const char *text);`
  - `void serialize_settings(const Settings &s, char *out, size_t out_size);`

- [ ] **Step 1: Write the header**

Create `src/settings/settings.h`:

```cpp
//
// src/settings/settings.h
//
// Bare Metal Sega Genesis
// Settings model: a typed struct plus pure parse/serialize over key=value text.
// No Circle dependencies — host-testable.
//

#ifndef _settings_settings_h
#define _settings_settings_h

#include <stddef.h>

enum class ScaleMode { Integer, Stretch };

struct Settings
{
    ScaleMode scale_mode;   // video_scale: integer | stretch
    bool      widescreen;   // widescreen:  on | off

    Settings(void) : scale_mode(ScaleMode::Integer), widescreen(false) {}
};

// Parse key=value text into a Settings. Missing/invalid/unknown keys fall back
// to defaults. '#' lines and blank lines are ignored. Never fails; text may be
// NULL (yields all defaults).
Settings parse_settings(const char *text);

// Render a Settings to key=value text (NUL-terminated, truncated to out_size).
void serialize_settings(const Settings &s, char *out, size_t out_size);

#endif
```

- [ ] **Step 2: Write the failing test**

Create `test/test_settings.cpp`:

```cpp
#include "../src/settings/settings.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    // Defaults for empty / NULL input.
    Settings d = parse_settings("");
    assert(d.scale_mode == ScaleMode::Integer);
    assert(d.widescreen == false);
    assert(parse_settings(0).scale_mode == ScaleMode::Integer);

    // Valid values.
    Settings a = parse_settings("video_scale=stretch\nwidescreen=on\n");
    assert(a.scale_mode == ScaleMode::Stretch);
    assert(a.widescreen == true);

    // Comments, blank lines, surrounding whitespace, case-insensitive.
    Settings b = parse_settings(
        "# comment\n\n  VIDEO_SCALE = Stretch  \nWidescreen=TRUE\n");
    assert(b.scale_mode == ScaleMode::Stretch);
    assert(b.widescreen == true);

    // Invalid value -> default; unknown key ignored; widescreen=0 -> false.
    Settings c = parse_settings("video_scale=bogus\nfoo=bar\nwidescreen=0\n");
    assert(c.scale_mode == ScaleMode::Integer);
    assert(c.widescreen == false);

    // Round-trip: serialize then parse yields equal settings.
    Settings src; src.scale_mode = ScaleMode::Stretch; src.widescreen = true;
    char buf[256];
    serialize_settings(src, buf, sizeof buf);
    Settings rt = parse_settings(buf);
    assert(rt.scale_mode == ScaleMode::Stretch);
    assert(rt.widescreen == true);

    printf("All settings tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Wire the test into `test/Makefile`**

In `test/Makefile`, add `test_settings` to the `run` target's prerequisites and command list, add a build rule, and add it to `clean`:

```makefile
run: test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings
	./test_blit
	./test_joypad
	./test_rom_filter
	./test_menu_state
	./test_menu_path
	./test_save_path
	./test_settings
```

Add this rule (after the `test_save_path` rule):

```makefile
test_settings: test_settings.cpp ../src/settings/settings.cpp ../src/settings/settings.h
	$(CXX) $(CXXFLAGS) -o $@ test_settings.cpp ../src/settings/settings.cpp
```

Update `clean`:

```makefile
clean:
	rm -f test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `cd test && make test_settings`
Expected: FAIL — link/compile error, `settings.cpp` does not exist yet (`No rule to make target ../src/settings/settings.cpp` or undefined references to `parse_settings`).

- [ ] **Step 5: Write the implementation**

Create `src/settings/settings.cpp`:

```cpp
//
// src/settings/settings.cpp
//
// Bare Metal Sega Genesis
// See settings.h.
//

#include "settings.h"
#include <string.h>

// Case-insensitive ASCII equality.
static bool ieq(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char) (ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char) (cb + 32);
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static bool truthy(const char *v)
{
    return ieq(v, "on") || ieq(v, "true") || ieq(v, "1") || ieq(v, "yes");
}

Settings parse_settings(const char *text)
{
    Settings s;                       // defaults
    if (text == 0) return s;

    const char *p = text;
    while (*p)
    {
        // Copy one line into a local buffer.
        char line[128];
        size_t n = 0;
        while (*p && *p != '\n' && *p != '\r')
        {
            if (n < sizeof(line) - 1) line[n++] = *p;
            p++;
        }
        line[n] = '\0';
        while (*p == '\n' || *p == '\r') p++;   // consume EOL(s)

        // Trim leading blanks; skip comments / empty lines.
        char *l = line;
        while (*l == ' ' || *l == '\t') l++;
        if (*l == '#' || *l == '\0') continue;

        // Split on '='.
        char *eq = strchr(l, '=');
        if (eq == 0) continue;
        *eq = '\0';
        char *key = l;
        char *val = eq + 1;

        // Trim trailing blanks on key.
        char *ke = key + strlen(key);
        while (ke > key && (ke[-1] == ' ' || ke[-1] == '\t')) *--ke = '\0';
        // Trim leading + trailing blanks on value.
        while (*val == ' ' || *val == '\t') val++;
        char *ve = val + strlen(val);
        while (ve > val && (ve[-1] == ' ' || ve[-1] == '\t')) *--ve = '\0';

        if (ieq(key, "video_scale"))
            s.scale_mode = ieq(val, "stretch") ? ScaleMode::Stretch
                                               : ScaleMode::Integer;
        else if (ieq(key, "widescreen"))
            s.widescreen = truthy(val);
        // unknown keys: ignored
    }
    return s;
}

// Append src to out without overflowing out_size (out must be NUL-terminated).
static void appendz(char *out, size_t out_size, const char *src)
{
    size_t len = strlen(out);
    size_t i   = 0;
    while (src[i] && len + 1 < out_size) out[len++] = src[i++];
    out[len] = '\0';
}

void serialize_settings(const Settings &s, char *out, size_t out_size)
{
    if (out == 0 || out_size == 0) return;
    out[0] = '\0';
    appendz(out, out_size, "# Bare Metal Sega Genesis settings\n");
    appendz(out, out_size, "video_scale=");
    appendz(out, out_size, s.scale_mode == ScaleMode::Stretch ? "stretch"
                                                              : "integer");
    appendz(out, out_size, "\nwidescreen=");
    appendz(out, out_size, s.widescreen ? "on" : "off");
    appendz(out, out_size, "\n");
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: `All settings tests passed`

- [ ] **Step 7: Add the test binary to `.gitignore`**

In `.gitignore`, under the "Compiled host test binaries" list, add:

```
/test/test_settings
```

- [ ] **Step 8: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp test/Makefile .gitignore
git commit -m "Settings: model + host-tested parse/serialize"
```

---

## Task 2: SettingsStore (load/save to SD card)

Thin glue over `Storage` to persist `Settings`. No host test (Circle-dependent, consistent with `sram`/`save_state`); verified by the cross build and on hardware.

**Files:**
- Create: `src/settings/settings_store.h`
- Create: `src/settings/settings_store.cpp`
- Modify: `Makefile` (add objects + EXTRACLEAN)

**Interfaces:**
- Consumes: `Settings`, `parse_settings`, `serialize_settings` (Task 1); `Storage::Exists/ReadFile/WriteFile` (`src/storage/storage.h`).
- Produces:
  - `class SettingsStore { SettingsStore(Storage*); bool Load(Settings*); bool Save(const Settings&); };`
  - `Load` returns `true` if a file was read, `false` if defaults were used (and a default file written). `Save` returns `false` on I/O failure.

- [ ] **Step 1: Write the header**

Create `src/settings/settings_store.h`:

```cpp
//
// src/settings/settings_store.h
//
// Bare Metal Sega Genesis
// Persists Settings to SD:/settings.txt via Storage. Separate from the Pi
// firmware's config.txt (which the GPU reads at boot — never written here).
//

#ifndef _settings_settings_store_h
#define _settings_settings_store_h

#include "settings.h"
#include "../storage/storage.h"

class SettingsStore
{
public:
    explicit SettingsStore(Storage *pStorage);

    // Read SD:/settings.txt into *pOut. If the file is missing or unreadable,
    // *pOut keeps default values and a default file is written. Returns true
    // if an existing file was read, false if defaults were used.
    bool Load(Settings *pOut);

    // Write the settings to SD:/settings.txt. Returns false on I/O failure.
    bool Save(const Settings &s);

private:
    Storage *m_pStorage;
};

#endif
```

- [ ] **Step 2: Write the implementation**

Create `src/settings/settings_store.cpp`:

```cpp
//
// src/settings/settings_store.cpp
//
// Bare Metal Sega Genesis
// See settings_store.h.
//

#include "settings_store.h"
#include <string.h>

static const char SETTINGS_PATH[] = "SD:/settings.txt";

SettingsStore::SettingsStore(Storage *pStorage)
:   m_pStorage(pStorage)
{
}

bool SettingsStore::Load(Settings *pOut)
{
    if (pOut == 0) return false;
    *pOut = Settings();               // defaults

    u8    *buf  = 0;
    size_t size = 0;
    if (!m_pStorage->Exists(SETTINGS_PATH) ||
        !m_pStorage->ReadFile(SETTINGS_PATH, &buf, &size))
    {
        Save(*pOut);                  // write a default file to edit
        return false;
    }

    // NUL-terminate a bounded copy for the parser.
    char   text[1024];
    size_t n = size < sizeof(text) - 1 ? size : sizeof(text) - 1;
    memcpy(text, buf, n);
    text[n] = '\0';
    delete[] buf;                     // Storage::ReadFile hands ownership over

    *pOut = parse_settings(text);
    return true;
}

bool SettingsStore::Save(const Settings &s)
{
    char text[256];
    serialize_settings(s, text, sizeof text);
    return m_pStorage->WriteFile(SETTINGS_PATH,
                                 (const u8 *) text, strlen(text));
}
```

- [ ] **Step 3: Add objects to the top-level `Makefile`**

In `Makefile`, add to the `OBJS` list (after `src/menu/sram.o`):

```makefile
       src/settings/settings.o \
       src/settings/settings_store.o \
```

And add the new directory to `EXTRACLEAN`:

```makefile
             src/settings/*.o src/settings/*.d \
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img` with no errors or warnings referencing `settings`.

- [ ] **Step 5: Commit**

```bash
git add src/settings/settings_store.h src/settings/settings_store.cpp Makefile
git commit -m "Settings: SettingsStore load/save to SD:/settings.txt"
```

---

## Task 3: Stretch blit (non-integer scale into a rect)

Pure, host-tested scaling primitive for the "stretch" video mode.

**Files:**
- Modify: `src/video/blit.h`
- Modify: `src/video/blit.cpp`
- Modify: `test/test_blit.cpp`

**Interfaces:**
- Produces:
  - `void blit_rgb565_scaled(uint16_t *dst, unsigned dst_pitch_bytes, unsigned dst_w, unsigned dst_h, const uint16_t *src, unsigned src_pitch_bytes, unsigned w, unsigned h, unsigned out_x, unsigned out_y, unsigned out_w, unsigned out_h);`
  - Nearest-neighbor scales the `w*h` source into the `out_w*out_h` rect at `(out_x,out_y)`. No-op if `src` is NULL, any of `w/h/out_w/out_h` is 0, or the rect leaves the surface. Caller clears bars.

- [ ] **Step 1: Declare the function in `blit.h`**

In `src/video/blit.h`, before the final `#endif`, add:

```cpp
// Nearest-neighbor scale a w*h RGB565 image into the destination rectangle
// (out_x, out_y, out_w, out_h) inside a dst_w*dst_h RGB565 surface. Used by the
// "stretch" video mode for non-integer aspect-fill scaling. Strides are in
// BYTES. No-op if src is NULL, any extent is 0, or the rect leaves the surface.
// The caller clears letterbox/pillarbox bars.
void blit_rgb565_scaled(uint16_t *dst, unsigned dst_pitch_bytes,
                        unsigned dst_w, unsigned dst_h,
                        const uint16_t *src, unsigned src_pitch_bytes,
                        unsigned w, unsigned h,
                        unsigned out_x, unsigned out_y,
                        unsigned out_w, unsigned out_h);
```

- [ ] **Step 2: Write the failing test**

In `test/test_blit.cpp`, add this test function before `int main(void)`:

```cpp
// blit_rgb565_scaled: 2x1 source stretched to a 4x2 rect, centered in 320x240.
static void test_scaled_stretch_rect(void) {
    reset_buffers();
    src[0] = 0xAAAA;                 // source (0,0)
    src[1] = 0xBBBB;                 // source (1,0)

    // Stretch a 2x1 image into a 4x2 rect at offset (10, 20).
    blit_rgb565_scaled(dst, DST_W * 2, DST_W, DST_H,
                       src, SRC_STRIDE_PX * 2, 2, 1,
                       10, 20, 4, 2);

    // Columns 0..1 map to source col 0; columns 2..3 map to source col 1.
    assert(dst[20 * DST_W + 10] == 0xAAAA);
    assert(dst[20 * DST_W + 11] == 0xAAAA);
    assert(dst[20 * DST_W + 12] == 0xBBBB);
    assert(dst[20 * DST_W + 13] == 0xBBBB);
    // Row 1 replicates row 0 (single source row).
    assert(dst[21 * DST_W + 10] == 0xAAAA);
    assert(dst[21 * DST_W + 13] == 0xBBBB);
    // Outside the rect stays clear.
    assert(dst[20 * DST_W + 9] == 0);
    printf("test_scaled_stretch_rect OK\n");
}

// blit_rgb565_scaled: rect that leaves the surface is a no-op.
static void test_scaled_out_of_bounds_noop(void) {
    reset_buffers();
    src[0] = 0x1234;
    blit_rgb565_scaled(dst, DST_W * 2, DST_W, DST_H,
                       src, SRC_STRIDE_PX * 2, 2, 2,
                       DST_W - 1, 0, 4, 4);   // exceeds width
    assert(dst[0] == 0);
    printf("test_scaled_out_of_bounds_noop OK\n");
}
```

And add these calls inside `main`, before `printf("All blit tests passed\n");`:

```cpp
    test_scaled_stretch_rect();
    test_scaled_out_of_bounds_noop();
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `cd test && make test_blit`
Expected: FAIL — `undefined reference to blit_rgb565_scaled`.

- [ ] **Step 4: Write the implementation**

In `src/video/blit.cpp`, append after `blit_rgb565`:

```cpp
void blit_rgb565_scaled(uint16_t *dst, unsigned dst_pitch_bytes,
                        unsigned dst_w, unsigned dst_h,
                        const uint16_t *src, unsigned src_pitch_bytes,
                        unsigned w, unsigned h,
                        unsigned out_x, unsigned out_y,
                        unsigned out_w, unsigned out_h)
{
    if (dst == 0 || src == 0) return;
    if (w == 0 || h == 0 || out_w == 0 || out_h == 0) return;
    if (out_x + out_w > dst_w || out_y + out_h > dst_h) return;  // doesn't fit

    unsigned dst_stride = dst_pitch_bytes / 2;   // pixels
    unsigned src_stride = src_pitch_bytes / 2;

    for (unsigned dy = 0; dy < out_h; dy++)
    {
        unsigned        sy   = dy * h / out_h;   // nearest-neighbor source row
        const uint16_t *srow = src + sy * src_stride;
        uint16_t       *drow = dst + (out_y + dy) * dst_stride + out_x;
        for (unsigned dx = 0; dx < out_w; dx++)
        {
            drow[dx] = srow[dx * w / out_w];
        }
    }
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd test && make test_blit && ./test_blit`
Expected: ends with `All blit tests passed` (including the two new `OK` lines).

- [ ] **Step 6: Commit**

```bash
git add src/video/blit.h src/video/blit.cpp test/test_blit.cpp
git commit -m "Video: blit_rgb565_scaled for stretch mode"
```

---

## Task 4: Display scale mode (integer vs stretch)

Wire the scale mode into `Display::Blit`, choosing the integer path (existing) or the stretch path (Task 3).

**Files:**
- Modify: `src/video/display.h`
- Modify: `src/video/display.cpp`

**Interfaces:**
- Consumes: `ScaleMode` (Task 1), `blit_rgb565_scaled` (Task 3).
- Produces: `void Display::SetScaleMode(ScaleMode mode);` (default `Integer`).

- [ ] **Step 1: Add the scale mode to the header**

In `src/video/display.h`, add the include after `#include <circle/types.h>`:

```cpp
#include "../settings/settings.h"   // ScaleMode
```

In the `public:` section, after `boolean Initialize(void);`, add:

```cpp
    // Select integer (sharp, letterboxed) vs stretch (aspect-fill) scaling.
    // Takes effect on the next Blit.
    void SetScaleMode(ScaleMode mode) { m_ScaleMode = mode; }
```

In the `private:` member list, after `unsigned m_LastH;`, add:

```cpp
    ScaleMode        m_ScaleMode;
```

- [ ] **Step 2: Initialize the member in the constructor**

In `src/video/display.cpp`, change the constructor init list to add `m_ScaleMode` (declaration order: it is the last member, so it goes last):

```cpp
Display::Display(void)
:   m_pFB(0), m_pBuffer(0), m_Pitch(0), m_FbW(0), m_FbH(0), m_LastW(0), m_LastH(0),
    m_ScaleMode(ScaleMode::Integer)
{
}
```

- [ ] **Step 3: Add the stretch path to `Blit`**

In `src/video/display.cpp`, in `Display::Blit`, replace the integer-scale block (from the `// Largest integer scale that fits the framebuffer.` comment through the closing `blit_rgb565(...)` call) with:

```cpp
    if (m_ScaleMode == ScaleMode::Stretch && width != 0 && height != 0)
    {
        // Largest 4:3 rectangle that fits the framebuffer, centered; the frame
        // is stretched (non-integer) to fill it.
        unsigned rw = m_FbW;
        unsigned rh = m_FbW * 3 / 4;
        if (rh > m_FbH) { rh = m_FbH; rw = m_FbH * 4 / 3; }
        unsigned ox = (m_FbW - rw) / 2;
        unsigned oy = (m_FbH - rh) / 2;
        blit_rgb565_scaled(m_pBuffer, m_Pitch, m_FbW, m_FbH,
                           (const uint16_t *) src, (unsigned) pitch,
                           width, height, ox, oy, rw, rh);
        return;
    }

    // Integer mode: largest whole scale that fits the framebuffer.
    unsigned scale = 1;
    if (width != 0 && height != 0)
    {
        unsigned sx = m_FbW / width;
        unsigned sy = m_FbH / height;
        scale = (sx < sy) ? sx : sy;
        if (scale < 1)
        {
            scale = 1;
        }
    }

    blit_rgb565(m_pBuffer, m_Pitch, m_FbW, m_FbH,
                (const uint16_t *) src, (unsigned) pitch, width, height, scale);
```

- [ ] **Step 4: Build to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no warnings/errors mentioning `display` or `ScaleMode`.

- [ ] **Step 5: Commit**

```bash
git add src/video/display.h src/video/display.cpp
git commit -m "Video: Display integer/stretch scale-mode selection"
```

---

## Task 5: Widescreen via environment callback

Make the libretro environment callback report the widescreen core variable from a global, and re-read on change.

**Files:**
- Modify: `src/libretro/environment.h`
- Modify: `src/libretro/environment.cpp`

**Interfaces:**
- Produces:
  - `extern bool g_widescreen;` (false => `"0"`, true => `"10"` for `genesis_plus_gx_wide_h40_extra_columns`)
  - `extern bool g_variables_dirty;` (set after a change so the core re-reads variables)

- [ ] **Step 1: Declare the globals in the header**

In `src/libretro/environment.h`, before the final `#endif`, add:

```cpp
// Genesis-Plus-GX-Wide widescreen toggle. Read by the GET_VARIABLE handler for
// "genesis_plus_gx_wide_h40_extra_columns": false => "0" (native 320-wide),
// true => "10" (extra columns). Set by the kernel/Settings screen.
extern bool g_widescreen;

// Set true after g_widescreen changes; the GET_VARIABLE_UPDATE handler reports
// and clears it so the core re-reads variables (applies on its next poll/reset).
extern bool g_variables_dirty;
```

- [ ] **Step 2: Define the globals**

In `src/libretro/environment.cpp`, after the line `size_t      g_rom_size = 0;`, add:

```cpp
// Video widescreen toggle + change flag (see environment.h).
bool g_widescreen      = false;
bool g_variables_dirty = false;
```

- [ ] **Step 3: Use the global in GET_VARIABLE**

In `src/libretro/environment.cpp`, in the `RETRO_ENVIRONMENT_GET_VARIABLE` case, replace the hardcoded value block:

```cpp
            retro_variable *var = reinterpret_cast<retro_variable *>(data);
            if (var != 0 && var->key != 0 &&
                strcmp(var->key, "genesis_plus_gx_wide_h40_extra_columns") == 0)
            {
                var->value = g_widescreen ? "10" : "0";
                return true;
            }
            return false;   // other options: core keeps its defaults
```

- [ ] **Step 4: Handle GET_VARIABLE_UPDATE**

In `src/libretro/environment.cpp`, add a new case before `default:`:

```cpp
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        {
            bool updated = g_variables_dirty;
            g_variables_dirty = false;
            if (data != 0) *reinterpret_cast<bool *>(data) = updated;
            return updated;
        }
```

- [ ] **Step 5: Build to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 6: Commit**

```bash
git add src/libretro/environment.h src/libretro/environment.cpp
git commit -m "Video: widescreen via environment GET_VARIABLE + update flag"
```

---

## Task 6: Settings screen (UI)

The in-emulation Settings screen: TextCanvas UI that edits `Settings`, applies changes, and persists them.

**Files:**
- Create: `src/menu/settings_screen.h`
- Create: `src/menu/settings_screen.cpp`
- Modify: `Makefile` (add object)

**Interfaces:**
- Consumes: `TextCanvas`, `Gamepad`, `CUSBHCIDevice`, `Settings` + `SettingsStore` (Tasks 1–2), `Display::SetScaleMode` (Task 4), `g_widescreen`/`g_variables_dirty` (Task 5), `GP_*` (`src/input/joypad_map.h`).
- Produces:
  - `class SettingsScreen { SettingsScreen(TextCanvas*, Gamepad*, CUSBHCIDevice*, Settings*, SettingsStore*, Display*); void Run(); };`

- [ ] **Step 1: Write the header**

Create `src/menu/settings_screen.h`:

```cpp
//
// src/menu/settings_screen.h
//
// Bare Metal Sega Genesis
// In-emulation Settings screen. Edits the shared Settings, applies changes
// (scale mode live via Display; widescreen via the environment globals), and
// persists to SD:/settings.txt through SettingsStore.
//

#ifndef _menu_settings_screen_h
#define _menu_settings_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"
#include "../video/display.h"

class SettingsScreen
{
public:
    SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay);
    void Run(void);   // returns when the user backs out (B)

private:
    void Render(int selected);
    void Apply(void);   // push current settings to Display + env globals

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
    Settings      *m_pSettings;
    SettingsStore *m_pStore;
    Display       *m_pDisplay;
};

#endif
```

- [ ] **Step 2: Write the implementation**

Create `src/menu/settings_screen.cpp`:

```cpp
//
// src/menu/settings_screen.cpp
//
// Bare Metal Sega Genesis
// See settings_screen.h.
//

#include "settings_screen.h"
#include "../input/joypad_map.h"          // GP_UP, GP_DOWN, GP_LEFT, GP_RIGHT, GP_B
#include "../libretro/environment.h"      // g_widescreen, g_variables_dirty
#include <circle/timer.h>

#define NUM_ROWS 2

// RGB565 colours (match the pause menu palette).
static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay)
{
}

void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_variables_dirty = true;                            // ...on next poll/reset
}

void SettingsScreen::Render(int selected)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2;
    int boxW = cw * 38, boxH = ch * (NUM_ROWS + 5);

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "SETTINGS", WHITE, BOX);

    const char *scaleVal = m_pSettings->scale_mode == ScaleMode::Stretch
                               ? "< Stretch >" : "< Integer >";
    const char *wideVal  = m_pSettings->widescreen ? "< On >" : "< Off >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal };

    for (int i = 0; i < NUM_ROWS; i++)
    {
        int  ty  = boxY + ch * (i + 3);
        bool sel = (i == selected);
        u16  fg  = sel ? SELFG : WHITE;
        u16  bg  = sel ? SELBG : BOX;
        if (sel) m_pCanvas->FillRect(boxX + cw, ty, boxW - cw * 2, ch, SELBG);
        m_pCanvas->DrawText(boxX + cw,      ty, sel ? ">" : " ", fg, bg);
        m_pCanvas->DrawText(boxX + cw * 3,  ty, labels[i],       fg, bg);
        m_pCanvas->DrawText(boxX + cw * 17, ty, values[i],       fg, bg);
    }

    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 4),
                        "Widescreen applies on reset.  B: back", WHITE, BOX);
}

void SettingsScreen::Run(void)
{
    int selected = 0;
    Render(selected);

    unsigned prev = m_pGamepad->Buttons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)
        {
            selected = (selected + NUM_ROWS - 1) % NUM_ROWS;
            Render(selected);
        }
        if (pressed & GP_DOWN)
        {
            selected = (selected + 1) % NUM_ROWS;
            Render(selected);
        }
        if (pressed & (GP_LEFT | GP_RIGHT))
        {
            if (selected == 0)
                m_pSettings->scale_mode =
                    m_pSettings->scale_mode == ScaleMode::Integer
                        ? ScaleMode::Stretch : ScaleMode::Integer;
            else
                m_pSettings->widescreen = !m_pSettings->widescreen;

            Apply();
            m_pStore->Save(*m_pSettings);
            Render(selected);
        }
        if (pressed & GP_B)
            return;

        CTimer::SimpleMsDelay(16);
    }
}
```

- [ ] **Step 3: Add the object to the `Makefile`**

In `Makefile`, add to `OBJS` (after `src/menu/sram.o`, alongside the other `src/menu/*.o`):

```makefile
       src/menu/settings_screen.o \
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 5: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp Makefile
git commit -m "Settings: in-emulation Settings screen UI"
```

---

## Task 7: Wire into pause menu + kernel (boot load + integration)

Enable the pause menu's "Settings" entry to open the screen, and have the kernel own the settings objects, load at boot, and apply them.

**Files:**
- Modify: `src/menu/pause_menu.h`
- Modify: `src/menu/pause_menu.cpp`
- Modify: `src/kernel.h`
- Modify: `src/kernel.cpp`

**Interfaces:**
- Consumes: `SettingsScreen` (Task 6), `Settings`/`SettingsStore` (Tasks 1–2), `g_widescreen` (Task 5), `Display::SetScaleMode` (Task 4).
- Produces: `PauseMenu` constructor gains a `SettingsScreen *pSettingsScreen` parameter.

- [ ] **Step 1: Add SettingsScreen to the pause menu header**

In `src/menu/pause_menu.h`, after the existing includes (before `enum class MenuAction`), add a forward declaration:

```cpp
class SettingsScreen;
```

Change the constructor signature and add the member. Replace:

```cpp
    PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
              SaveState *pSaveState);
```

with:

```cpp
    PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
              SaveState *pSaveState, SettingsScreen *pSettingsScreen);
```

In the `private:` member list, after `SaveState *m_pSaveState;`, add:

```cpp
    SettingsScreen *m_pSettingsScreen;
```

- [ ] **Step 2: Update the pause menu implementation**

In `src/menu/pause_menu.cpp`, add the include after the existing includes:

```cpp
#include "settings_screen.h"
```

Enable the "Settings" entry — change the `ENABLED` array:

```cpp
static const bool ENABLED[NUM_ENTRIES] = { true, true, true, true, true, true };
```

Update the constructor:

```cpp
PauseMenu::PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                     SaveState *pSaveState, SettingsScreen *pSettingsScreen)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSaveState(pSaveState), m_pSettingsScreen(pSettingsScreen)
{
}
```

In `PauseMenu::Run`, in the `switch (selected)`, add a `case 4` before `case 5`:

```cpp
            case 4:                       // Settings
                m_pSettingsScreen->Run();
                Render(selected);
                prev = m_pGamepad->Buttons();
                break;
```

- [ ] **Step 3: Declare the new members in the kernel header**

In `src/kernel.h`, add the includes near the other menu/subsystem includes (e.g. after `#include "menu/rom_menu.h"`):

```cpp
#include "settings/settings.h"
#include "settings/settings_store.h"
#include "menu/settings_screen.h"
```

In the member list, immediately **before** `PauseMenu m_PauseMenu;` (so they are constructed before it), add:

```cpp
	Settings           m_Settings;       // user settings (video, etc.)
	SettingsStore      m_SettingsStore;  // load/save settings to SD card
	SettingsScreen     m_SettingsScreen; // in-emulation Settings screen
```

(Declaration order: these must appear after `m_Display`, `m_Gamepad`, `m_Storage`, and `m_Canvas`, which they reference, and before `m_PauseMenu`. The slot just above `m_PauseMenu` satisfies all of these.)

- [ ] **Step 4: Update the kernel constructor init list**

In `src/kernel.cpp`, in the constructor init list, replace the `m_PauseMenu (...)` line with the new objects followed by the updated `m_PauseMenu` (order must match declaration order):

```cpp
	m_Settings (),
	m_SettingsStore (&m_Storage),
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display),
	m_PauseMenu (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_SaveState, &m_SettingsScreen),
```

- [ ] **Step 5: Load and apply settings at boot**

In `src/kernel.cpp`, in `CKernel::Run`, immediately after the SD mount success (after the `if (!m_Storage.Mount ()) { ... }` block, before `retro_set_environment (environment_cb);`), add:

```cpp
	// Load user settings and apply them before the core reads variables.
	m_SettingsStore.Load (&m_Settings);
	m_Display.SetScaleMode (m_Settings.scale_mode);
	g_widescreen = m_Settings.widescreen;
```

(`g_widescreen` is declared in `src/libretro/environment.h`, which `kernel.cpp` already includes.)

- [ ] **Step 6: Build to verify everything compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors or `-Wreorder` warnings.

- [ ] **Step 7: Run the full host test suite**

Run: `cd test && make run`
Expected: all suites pass, ending with `All settings tests passed` and `All blit tests passed`.

- [ ] **Step 8: Commit**

```bash
git add src/menu/pause_menu.h src/menu/pause_menu.cpp src/kernel.h src/kernel.cpp
git commit -m "Settings: wire Settings screen into pause menu + kernel boot load"
```

---

## Hardware Verification (manual, after Task 7)

Not a code task — perform on the Pi per `docs/m5-hardware-setup.md`:

- [ ] Boot; confirm `SD:/settings.txt` is created with defaults on first run.
- [ ] In a game, open the pause menu (Start+Select) → "Settings".
- [ ] Toggle **Video Scale** Integer ↔ Stretch with left/right; confirm the picture changes live on return to the game.
- [ ] Toggle **Widescreen** On, choose Reset Game; confirm wider rendering. Toggle Off + Reset; confirm 320-wide.
- [ ] Power-cycle; confirm the chosen values persisted (re-open Settings and/or inspect `SD:/settings.txt`).

---

## Self-Review Notes

- **Spec coverage:** Settings module (Task 1–2), `SD:/settings.txt` separate file (Task 2), `video_scale` integer/stretch live (Tasks 3–4), widescreen on reset (Task 5), Settings screen off pause menu (Tasks 6–7), boot load + defaults + best-effort save (Tasks 2, 7), host tests (Tasks 1, 3). Output HDMI mode is explicitly out of scope (deferred spec) — no task, as intended.
- **Type consistency:** `ScaleMode`/`Settings` defined in Task 1 and consumed unchanged in Tasks 4/6/7; `parse_settings`/`serialize_settings` signatures match across Tasks 1–2; `blit_rgb565_scaled` signature identical in Tasks 3–4; `g_widescreen`/`g_variables_dirty` defined in Task 5 and used in Tasks 6–7; `SettingsScreen` and `PauseMenu` constructor signatures consistent across Tasks 6–7.
