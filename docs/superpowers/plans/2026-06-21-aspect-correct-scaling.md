# Aspect-Correct Video Scaling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a third `video_scale` mode, `aspect`, that displays Genesis content at true 4:3 proportions (vertical integer scale + horizontal stretch-to-4:3) while keeping vertical resolution crisp.

**Architecture:** A pure, host-testable `aspect_rect()` helper in `src/video/blit.{h,cpp}` computes the output rectangle; `Display::Blit()` gains an `Aspect` branch that feeds that rect to the existing `blit_rgb565_scaled` (no new blit primitive). The `ScaleMode` enum (shared by settings and display) gains `Aspect`; parse/serialize and the settings-screen toggle are extended to a 3-way choice. Default stays `Integer`.

**Tech Stack:** C++ (bare-metal Circle for device code; host C++ with the system compiler for unit tests via `test/Makefile`). RGB565 framebuffer.

## Global Constraints

- Pure helpers in `src/video/blit.*` and `src/settings/*` must have **no Circle dependency** (host-testable; `#include <stdint.h>`/`<stddef.h>` only).
- `ScaleMode` is a single shared enum in `src/settings/settings.h`; `Display` consumes it via include — do **not** duplicate it.
- `Integer` remains the constructor/fallback default; `aspect` is opt-in. Unknown `video_scale` values fall back to `integer`.
- Strides/pitches are in **bytes** at the blit API boundary; the core's source pitch is fixed-width (720 px) while active width varies.
- Host tests are built and run with `make -C test run`.

---

### Task 1: `aspect_rect()` geometry helper (pure, host-tested)

**Files:**
- Modify: `src/video/blit.h` (add declaration)
- Modify: `src/video/blit.cpp` (add definition)
- Test: `test/test_blit.cpp` (add cases + call them from `main`)

**Interfaces:**
- Consumes: nothing.
- Produces: `void aspect_rect(unsigned fb_w, unsigned fb_h, unsigned w, unsigned h, unsigned *out_x, unsigned *out_y, unsigned *out_w, unsigned *out_h);`
  - Computes the largest 4:3 box that fits `fb_w×fb_h`, sets `*out_h = h * Sv` where `Sv = max(1, rh / h)` (`rh` = that box's height), `*out_w = min(fb_w, *out_h * 4 / 3)`, centered. If `w==0 || h==0 || fb_w==0 || fb_h==0`, all four outputs are set to 0.

- [ ] **Step 1: Write the failing tests**

Add to `test/test_blit.cpp` (above `main`, after the existing static test functions):

```cpp
#include "../src/video/blit.h"  // already included at top; no-op if present

// 320x224 and 256x224 into 1920x1080 must produce the SAME width and a 4:3 box.
static void test_aspect_h32_h40_same_width(void) {
    unsigned x40, y40, w40, h40, x32, y32, w32, h32;
    aspect_rect(1920, 1080, 320, 224, &x40, &y40, &w40, &h40);
    aspect_rect(1920, 1080, 256, 224, &x32, &y32, &w32, &h32);

    // Same source height -> identical output box for both widths.
    assert(w40 == w32);
    assert(h40 == h32);
    // Vertical scale is integer: out_h is a whole multiple of source height.
    assert(h40 % 224 == 0);
    // Box is ~4:3 (within integer rounding of the /3 *4).
    assert(w40 * 3 <= h40 * 4 && (w40 + 4) * 3 >= h40 * 4);
    // Fits and is centered.
    assert(x40 + w40 <= 1920 && y40 + h40 <= 1080);
    assert(x40 == (1920 - w40) / 2 && y40 == (1080 - h40) / 2);
    printf("test_aspect_h32_h40_same_width OK\n");
}

// Vertical integer scale picks the largest whole factor fitting the 4:3 box.
static void test_aspect_vertical_integer(void) {
    unsigned x, y, w, h;
    aspect_rect(1920, 1080, 320, 224, &x, &y, &w, &h);
    // 4:3 box in 1920x1080 is 1440x1080; 1080/224 = 4 -> out_h = 896.
    assert(h == 896);
    assert(w == 896 * 4 / 3);  // 1194
    printf("test_aspect_vertical_integer OK\n");
}

// Degenerate inputs are safe (all zeros).
static void test_aspect_degenerate(void) {
    unsigned x = 9, y = 9, w = 9, h = 9;
    aspect_rect(1920, 1080, 0, 224, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);
    aspect_rect(0, 0, 320, 224, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);
    printf("test_aspect_degenerate OK\n");
}
```

Then add these three calls inside `main` (next to the other `test_*();` calls):

```cpp
    test_aspect_h32_h40_same_width();
    test_aspect_vertical_integer();
    test_aspect_degenerate();
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -C test test_blit && ./test/test_blit`
Expected: FAIL — compile error `'aspect_rect' was not declared in this scope`.

- [ ] **Step 3: Declare `aspect_rect` in `src/video/blit.h`**

Add before the closing `#endif`:

```cpp
// Compute the output rectangle for "aspect" scaling: a true 4:3 box with
// VERTICAL INTEGER scaling (crisp row-replication) and the source width
// stretched to the 4:3 width. Given framebuffer fb_w*fb_h and source w*h,
// writes the centered destination rect (out_x,out_y,out_w,out_h). All four
// outputs are 0 if any input dimension is 0. Pure; no Circle dependency.
void aspect_rect(unsigned fb_w, unsigned fb_h, unsigned w, unsigned h,
                 unsigned *out_x, unsigned *out_y,
                 unsigned *out_w, unsigned *out_h);
```

- [ ] **Step 4: Define `aspect_rect` in `src/video/blit.cpp`**

Add at the end of the file:

```cpp
void aspect_rect(unsigned fb_w, unsigned fb_h, unsigned w, unsigned h,
                 unsigned *out_x, unsigned *out_y,
                 unsigned *out_w, unsigned *out_h)
{
    *out_x = *out_y = *out_w = *out_h = 0;
    if (w == 0 || h == 0 || fb_w == 0 || fb_h == 0) return;

    // Largest 4:3 rectangle that fits the framebuffer.
    unsigned rh = fb_w * 3 / 4;
    if (rh > fb_h) rh = fb_h;     // height-bound; rw would be rh*4/3 (implicit)

    // Vertical: largest integer factor that fits the 4:3 box height.
    unsigned sv = rh / h;
    if (sv < 1) sv = 1;
    unsigned oh = h * sv;

    // Horizontal: stretch to 4:3 width, clamped to the framebuffer.
    unsigned ow = oh * 4 / 3;
    if (ow > fb_w) ow = fb_w;

    *out_w = ow;
    *out_h = oh;
    *out_x = (fb_w - ow) / 2;
    *out_y = (fb_h - oh) / 2;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `make -C test test_blit && ./test/test_blit`
Expected: PASS — including the three new `test_aspect_* OK` lines.

- [ ] **Step 6: Commit**

```bash
git add src/video/blit.h src/video/blit.cpp test/test_blit.cpp
git commit -m "Video: add aspect_rect() 4:3 geometry helper (host-tested)"
```

---

### Task 2: `ScaleMode::Aspect` enum + settings parse/serialize

**Files:**
- Modify: `src/settings/settings.h:14` (enum)
- Modify: `src/settings/settings.cpp` (parse ~line 69, serialize ~line 289)
- Test: `test/test_settings.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: `ScaleMode::Aspect` enumerator; `video_scale=aspect` ⇄ `ScaleMode::Aspect` round-trip.

- [ ] **Step 1: Write the failing tests**

Add inside `main` in `test/test_settings.cpp`, after the existing `video_scale` assertions:

```cpp
    // Aspect mode parses and round-trips.
    Settings asp = parse_settings("video_scale=aspect\n");
    assert(asp.scale_mode == ScaleMode::Aspect);

    Settings aspSrc; aspSrc.scale_mode = ScaleMode::Aspect;
    char aspBuf[256];
    serialize_settings(aspSrc, aspBuf, sizeof aspBuf);
    assert(parse_settings(aspBuf).scale_mode == ScaleMode::Aspect);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -C test test_settings && ./test/test_settings`
Expected: FAIL — compile error: `'Aspect' is not a member of 'ScaleMode'`.

- [ ] **Step 3: Add the enumerator**

In `src/settings/settings.h:14` change:

```cpp
enum class ScaleMode { Integer, Stretch };
```
to:
```cpp
enum class ScaleMode { Integer, Stretch, Aspect };
```

- [ ] **Step 4: Extend parse and serialize**

In `src/settings/settings.cpp`, replace the parse block (currently lines ~69-71):

```cpp
        if (ieq(key, "video_scale"))
            s.scale_mode = ieq(val, "stretch") ? ScaleMode::Stretch
                                               : ScaleMode::Integer;
```
with:
```cpp
        if (ieq(key, "video_scale"))
            s.scale_mode = ieq(val, "stretch") ? ScaleMode::Stretch
                         : ieq(val, "aspect")  ? ScaleMode::Aspect
                                               : ScaleMode::Integer;
```

And replace the serialize block (currently lines ~289-291):

```cpp
    appendz(out, out_size, "video_scale=");
    appendz(out, out_size, s.scale_mode == ScaleMode::Stretch ? "stretch"
                                                              : "integer");
```
with:
```cpp
    appendz(out, out_size, "video_scale=");
    appendz(out, out_size,
            s.scale_mode == ScaleMode::Stretch ? "stretch"
          : s.scale_mode == ScaleMode::Aspect  ? "aspect"
                                               : "integer");
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_settings && ./test/test_settings`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add video_scale=aspect (parse/serialize + host test)"
```

---

### Task 3: `Display::Blit()` Aspect branch

**Files:**
- Modify: `src/video/display.cpp` (add branch in `Blit()`, ~after the Stretch branch at lines 164-178)

**Interfaces:**
- Consumes: `aspect_rect(...)` from Task 1; `ScaleMode::Aspect` from Task 2; existing `blit_rgb565_scaled(...)`.
- Produces: device-side aspect rendering. No host test (device-only `Display`; geometry is already covered by Task 1).

- [ ] **Step 1: Add the Aspect branch**

In `src/video/display.cpp`, immediately AFTER the Stretch branch (the block ending with the `return;` at line 178, before the `// Integer mode:` comment), insert:

```cpp
    if (m_ScaleMode == ScaleMode::Aspect && width != 0 && height != 0)
    {
        // True 4:3 box: vertical integer (crisp), horizontal stretch to 4:3.
        unsigned ox, oy, ow, oh;
        aspect_rect(m_FbW, m_FbH, width, height, &ox, &oy, &ow, &oh);
        blit_rgb565_scaled(target, m_Pitch, m_FbW, m_FbH,
                           (const uint16_t *) src, (unsigned) pitch,
                           width, height, ox, oy, ow, oh);
        if (flip) Present();
        return;
    }
```

(`blit.h` is already included at the top of `display.cpp`, so `aspect_rect` is in scope.)

- [ ] **Step 2: Build the kernel to verify it compiles**

Run: `make` (from repo root)
Expected: builds to completion (kernel image produced), no errors referencing `display.cpp` or `aspect_rect`.

> If the full cross build is unavailable in this environment, instead verify the
> branch compiles in isolation:
> `c++ -fsyntax-only -I libs/genesis-plus-gx-wide/libretro/libretro-common/include src/video/display.cpp`
> is NOT expected to pass (Circle headers), so rely on `make`. If `make` cannot
> run here, leave this step for the hardware/build host and note it in the commit.

- [ ] **Step 3: Commit**

```bash
git add src/video/display.cpp
git commit -m "Video: render aspect mode (4:3, vertical-integer) in Display::Blit"
```

---

### Task 4: Settings screen — 3-way cycle + label

**Files:**
- Modify: `src/menu/settings_screen.cpp` (label ~lines 72-73; cycle ~lines 150-154)

**Interfaces:**
- Consumes: `ScaleMode::Aspect`.
- Produces: user-visible `< Aspect >` row value and a 3-way Left/Right cycle. No host test (UI rendering; logic is a trivial enum cycle).

- [ ] **Step 1: Update the displayed value (3-way label)**

In `src/menu/settings_screen.cpp`, replace (lines ~72-73):

```cpp
    const char *scaleVal = m_pSettings->scale_mode == ScaleMode::Stretch
                               ? "< Stretch >" : "< Integer >";
```
with:
```cpp
    const char *scaleVal =
        m_pSettings->scale_mode == ScaleMode::Stretch ? "< Stretch >" :
        m_pSettings->scale_mode == ScaleMode::Aspect  ? "< Aspect >"  :
                                                        "< Integer >";
```

- [ ] **Step 2: Update the toggle to a 3-way cycle**

Replace the `case 0:` block (lines ~150-154):

```cpp
            case 0:   // Video Scale (toggle, either direction)
                m_pSettings->scale_mode =
                    m_pSettings->scale_mode == ScaleMode::Integer
                        ? ScaleMode::Stretch : ScaleMode::Integer;
                break;
```
with:
```cpp
            case 0:   // Video Scale: cycle Integer -> Stretch -> Aspect
            {
                // dir is +1 (Right) or -1 (Left); wrap through the 3 modes.
                int n = (int) m_pSettings->scale_mode + (dir > 0 ? 1 : 2);
                m_pSettings->scale_mode = (ScaleMode) (n % 3);
                break;
            }
```

- [ ] **Step 3: Build to verify it compiles**

Run: `make` (from repo root)
Expected: builds to completion, no errors in `settings_screen.cpp`.

- [ ] **Step 4: Run the full host test suite (regression)**

Run: `make -C test run`
Expected: all tests print `OK` / pass; no regressions.

- [ ] **Step 5: Commit**

```bash
git add src/menu/settings_screen.cpp
git commit -m "Settings UI: 3-way video scale cycle with Aspect label"
```

---

### Task 5: Hardware-verify checklist entry

**Files:**
- Modify: the project hardware-verification checklist doc (the file containing the existing "section K" tear-free checklist — locate with `grep -rl "section K\|Vsync" docs/`).

**Interfaces:** none (documentation).

- [ ] **Step 1: Locate the checklist file**

Run: `grep -rln "Tear-free\|Vsync\|section K" docs/`
Expected: prints the checklist doc path (the one tracking pending hardware verification).

- [ ] **Step 2: Append an Aspect-mode verification section**

Add a new checklist section (mirror the formatting of the existing sections in that file):

```markdown
## Section L — Aspect-correct scaling

- [ ] `video_scale=aspect` is selectable in Settings (Left/Right cycles
      Integer -> Stretch -> Aspect) and the `< Aspect >` label shows.
- [ ] Selection applies live (no reboot) and persists across a reboot.
- [ ] An H40/320-wide game (e.g. Sonic) and an H32/256-wide game fill the
      SAME display width and look correctly proportioned (not thin/tall).
- [ ] Vertical edges are crisp (integer scale); no excessive horizontal blur.
- [ ] Frame rate at the chosen video_mode is acceptable vs. stretch
      (cap to 720p if 1080p is not smooth).
```

- [ ] **Step 3: Commit**

```bash
git add docs/
git commit -m "Docs: add aspect-scaling hardware-verify checklist (section L)"
```

---

## Self-Review Notes

- **Spec coverage:** geometry rule → Task 1; enum + parse/serialize → Task 2; `Display::Blit` branch → Task 3; settings UI 3-way + label → Task 4; host tests → Tasks 1-2 + regression in Task 4; hardware-verify checklist → Task 5. All spec sections mapped.
- **Type consistency:** `aspect_rect(unsigned, unsigned, unsigned, unsigned, unsigned*, unsigned*, unsigned*, unsigned*)` and `ScaleMode::Aspect` are used identically across Tasks 1, 3, 4.
- **No new blit primitive** — Task 3 reuses `blit_rgb565_scaled`; vertical crispness comes from `out_h` being an exact multiple of `h` (verified by `test_aspect_vertical_integer`).
