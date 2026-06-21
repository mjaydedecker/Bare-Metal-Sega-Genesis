# Tear-Free Video Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate gameplay tearing by rendering each frame to an off-screen page and presenting it with a vsync-synced page flip, gated by a `vsync` setting (default on).

**Architecture:** All real work is contained in `Display`: the framebuffer becomes two stacked pages (virtual height = 2× physical); `Blit()` renders the game frame into the off-screen (back) page then flips at vblank, while `Buffer()` keeps returning the visible (front) page so menus/UI are untouched. A `vsync` setting toggles the path live; when off (or if the two-page allocation fails) it falls back to today's single-buffer direct draw.

**Tech Stack:** C++ (bare-metal, Circle framework), `CBcmFrameBuffer` virtual-offset page flipping; arm-none-eabi cross toolchain; system `c++` for host unit tests.

## Global Constraints

- Target board: **Pi 2** (`RASPPI=2`).
- `vsync` setting default **on**; persisted in `SD:/settings.txt`; toggle is **live**.
- Pacing stays the timer + audio-queue gate in `kernel.cpp` — the vsync wait only aligns the flip and must NOT replace the frame clock. Do not touch the kernel pacing loop.
- No changes to `blit.cpp` scaling math, the libretro `video_refresh_cb`, or any menu/UI file beyond adding the one settings row.
- Graceful degradation: if the two-page framebuffer can't be allocated, behave exactly as today (single page, no flip). `Initialize()`/`SetMode()` keep their keep-on-fail contracts (build new FB before discarding the old).
- Settings model (`src/settings/settings.{h,cpp}`) stays host-testable, no Circle deps.

---

### Task 1: `vsync` setting (parse/serialize)

Adds the `Settings::vsync` bool, parse/serialize, and host tests. Mirrors the existing bool toggles (`mute`, `widescreen`) which use the shared `truthy()` parser.

**Files:**
- Modify: `src/settings/settings.h`
- Modify: `src/settings/settings.cpp`
- Test: `test/test_settings.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `Settings::vsync` (type `bool`, default `true`); settings text key `vsync=on|off`.

- [ ] **Step 1: Write the failing tests**

In `test/test_settings.cpp`, insert this block immediately before the final
`printf("All settings tests passed\n");` line:

```cpp
    // Vsync defaults on; parses off/on; round-trips (truthy parse, like mute).
    assert(d.vsync == true);
    assert(parse_settings("vsync=off\n").vsync == false);
    assert(parse_settings("vsync=on\n").vsync  == true);
    Settings vs2; vs2.vsync = false;
    char vsbuf[512];
    serialize_settings(vs2, vsbuf, sizeof vsbuf);
    assert(parse_settings(vsbuf).vsync == false);
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `make -C test test_settings`
Expected: FAIL — `struct Settings` has no member named `vsync`.

- [ ] **Step 3: Add the field and default**

In `src/settings/settings.h`, add the field to the `Settings` struct after the
`audio_output` member:

```cpp
    AudioOutput audio_output;  // hdmi | analog (3.5mm jack)
    bool       vsync;          // tear-free page flip (default on)
```

Add the default to the constructor initializer list — change:

```cpp
        menu_hotkey(MenuHotkey::StartSelect), video_mode(VideoMode::Native),
        audio_output(AudioOutput::HDMI)
```

to:

```cpp
        menu_hotkey(MenuHotkey::StartSelect), video_mode(VideoMode::Native),
        audio_output(AudioOutput::HDMI), vsync(true)
```

- [ ] **Step 4: Add parse and serialize**

In `src/settings/settings.cpp`, add a parse branch immediately after the
`audio_output` branch (before the `// unknown keys: ignored` comment):

```cpp
        else if (ieq(key, "vsync"))
            s.vsync = truthy(val);
```

In `serialize_settings`, append the field after the `audio_output` block (before
the final `appendz(out, out_size, "\n");`):

```cpp
    appendz(out, out_size, "\nvsync=");
    appendz(out, out_size, s.vsync ? "on" : "off");
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_settings && ./test/test_settings`
Expected: PASS — prints `All settings tests passed`.

- [ ] **Step 6: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add vsync toggle (default on) parse/serialize + host test

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Display double-buffering + vsync page flip

The core change. `Display` gains a two-page framebuffer, a front/back model, a
`Present()` flip, and `SetVsync()`. Not host-testable (Circle/hardware); the
cross build is the gate.

**Files:**
- Modify: `src/video/display.h`
- Modify: `src/video/display.cpp`

**Interfaces:**
- Consumes: nothing from Task 1 (reads its own `m_Vsync`, default true).
- Produces:
  - `void Display::SetVsync(bool on)` — live toggle, effective next `Blit`.
  - `Buffer()` now returns the visible (front) page.
  - Behavior: when vsync on AND two pages allocated, `Blit` renders to the back
    page and flips at vblank; otherwise it draws to the front page directly
    (today's behavior).

- [ ] **Step 1: Rewrite the header**

Replace the body of `src/video/display.h` (the `class Display { ... };` block,
lines 21-67) with:

```cpp
class Display
{
public:
    static const unsigned FB_DEPTH = 16;     // RGB565

    Display(void);
    ~Display(void);

    boolean Initialize(void);

    // Re-create the framebuffer at a new HDMI mode (w=h=0 => firmware native).
    // On success swaps to the new mode and returns TRUE; on failure keeps the
    // current mode and returns FALSE. NOTE: success does not prove the TV
    // accepts the signal — callers must confirm-or-revert.
    boolean SetMode(unsigned w, unsigned h);

    // Select integer (sharp, letterboxed) vs stretch (aspect-fill) scaling.
    void SetScaleMode(ScaleMode mode) { m_ScaleMode = mode; }

    // Enable/disable the tear-free vsync page flip. Live; takes effect on the
    // next Blit. When off (or if double-buffering is unavailable) Blit draws
    // straight to the visible page with no flip.
    void SetVsync(bool on) { m_Vsync = on; }

    // Copy one RGB565 frame, integer-scaled and centered. pitch is the source
    // row stride in bytes.
    void Blit(const void *src, unsigned width, unsigned height, size_t pitch);

    // Framebuffer accessors for on-screen UI (TextCanvas draws into the visible
    // page). Game frames go through Blit(), never Buffer().
    u16     *Buffer(void) const { return m_pFront; }
    unsigned Pitch (void) const { return m_Pitch; }   // bytes per row
    unsigned Width (void) const { return m_FbW; }     // pixels
    unsigned Height(void) const { return m_FbH; }     // pixels

    // Force the next Blit to clear the framebuffer (repaint letterbox bars).
    void ForceRepaint(void) { m_LastW = 0; m_LastH = 0; }

private:
    void ClearBlack(void);                 // clears both pages when double-buffered
    void Present(void);                    // vsync flip + swap front/back
    CBcmFrameBuffer *BuildFB(unsigned reqW, unsigned reqH, bool &doubled);
    void Adopt(CBcmFrameBuffer *fb, bool doubled);

    CBcmFrameBuffer *m_pFB;
    u16             *m_pFront;        // visible page base (UI + non-vsync draw)
    u16             *m_pBack;         // off-screen page base (== front if single)
    bool             m_FrontIsPage0;  // which physical page is currently visible
    bool             m_DoubleBuffered;
    bool             m_Vsync;
    unsigned         m_Pitch;         // framebuffer pitch in bytes
    unsigned         m_FbW;           // framebuffer width in pixels
    unsigned         m_FbH;           // framebuffer height in pixels
    unsigned         m_LastW;
    unsigned         m_LastH;
    ScaleMode        m_ScaleMode;
};
```

- [ ] **Step 2: Rewrite the constructor and framebuffer setup**

In `src/video/display.cpp`, replace the constructor, destructor, `Initialize`,
and `SetMode` (lines 13-76) with:

```cpp
Display::Display(void)
:   m_pFB(0), m_pFront(0), m_pBack(0), m_FrontIsPage0(true),
    m_DoubleBuffered(false), m_Vsync(true),
    m_Pitch(0), m_FbW(0), m_FbH(0), m_LastW(0), m_LastH(0),
    m_ScaleMode(ScaleMode::Integer)
{
}

Display::~Display(void)
{
    delete m_pFB;
    m_pFB = 0;
}

// Build a framebuffer for the requested mode (0,0 => firmware's current mode),
// preferring a two-page (double-height virtual) surface for tear-free flipping.
// Returns 0 on total failure; sets `doubled` to whether two pages were obtained.
CBcmFrameBuffer *Display::BuildFB(unsigned reqW, unsigned reqH, bool &doubled)
{
    unsigned w = reqW, h = reqH;

    // For the firmware's current mode (0,0) we must learn its dimensions before
    // we can request a double-height virtual framebuffer.
    if (w == 0 || h == 0)
    {
        CBcmFrameBuffer *probe = new CBcmFrameBuffer(reqW, reqH, FB_DEPTH);
        if (probe != 0 && probe->Initialize() && probe->GetBuffer() != 0)
        {
            w = probe->GetWidth();
            h = probe->GetHeight();
        }
        delete probe;
        if (w == 0 || h == 0)
        {
            doubled = false;
            return 0;
        }
    }

    // Prefer two pages: virtual height = 2x physical.
    CBcmFrameBuffer *fb = new CBcmFrameBuffer(w, h, FB_DEPTH, w, 2 * h);
    if (fb != 0 && fb->Initialize() && fb->GetBuffer() != 0)
    {
        doubled = true;
        return fb;
    }
    delete fb;

    // Fall back to a single page (today's behavior).
    fb = new CBcmFrameBuffer(w, h, FB_DEPTH);
    if (fb != 0 && fb->Initialize() && fb->GetBuffer() != 0)
    {
        doubled = false;
        return fb;
    }
    delete fb;
    doubled = false;
    return 0;
}

// Wire the display state to a freshly-built framebuffer. The new FB defaults to
// virtual offset (0,0), so page 0 is visible (front).
void Display::Adopt(CBcmFrameBuffer *fb, bool doubled)
{
    m_pFB            = fb;
    m_Pitch          = fb->GetPitch();
    m_FbW            = fb->GetWidth();
    m_FbH            = fb->GetHeight();
    m_DoubleBuffered = doubled;
    m_FrontIsPage0   = true;

    u16 *page0 = (u16 *) (uintptr_t) fb->GetBuffer();
    m_pFront = page0;
    m_pBack  = doubled
        ? (u16 *) ((u8 *) page0 + (size_t) m_Pitch * m_FbH)
        : page0;

    m_LastW = 0;
    m_LastH = 0;
    ClearBlack();
}

boolean Display::Initialize(void)
{
    bool doubled = false;
    CBcmFrameBuffer *fb = BuildFB(0, 0, doubled);
    if (fb == 0)
    {
        return FALSE;
    }
    Adopt(fb, doubled);
    return TRUE;
}

boolean Display::SetMode(unsigned w, unsigned h)
{
    // Build the new framebuffer BEFORE discarding the current one, so a failure
    // leaves the existing mode intact.
    bool doubled = false;
    CBcmFrameBuffer *pNew = BuildFB(w, h, doubled);
    if (pNew == 0)
    {
        return FALSE;
    }
    delete m_pFB;
    Adopt(pNew, doubled);
    return TRUE;
}
```

- [ ] **Step 3: Update ClearBlack and add Present**

In `src/video/display.cpp`, replace the `ClearBlack` function (lines 78-84) with
the following (clears both pages when double-buffered, and adds `Present`):

```cpp
void Display::ClearBlack(void)
{
    if (m_pFront != 0)
    {
        memset(m_pFront, 0, (size_t) m_Pitch * m_FbH);
    }
    if (m_DoubleBuffered && m_pBack != 0)
    {
        memset(m_pBack, 0, (size_t) m_Pitch * m_FbH);
    }
}

// Flip at vblank: wait for vsync, pan to the page we just drew (the back page),
// then swap front/back so the next Blit targets the now-hidden page.
void Display::Present(void)
{
    m_pFB->WaitForVerticalSync();
    m_pFB->SetVirtualOffset(0, m_FrontIsPage0 ? m_FbH : 0);

    u16 *tmp = m_pFront;
    m_pFront = m_pBack;
    m_pBack  = tmp;
    m_FrontIsPage0 = !m_FrontIsPage0;
}
```

- [ ] **Step 4: Update Blit to target the back page and flip**

In `src/video/display.cpp`, replace the `Blit` function (lines 86-130) with:

```cpp
void Display::Blit(const void *src, unsigned width, unsigned height, size_t pitch)
{
    if (m_pFront == 0 || src == 0)   // no surface, or dupe frame
    {
        return;
    }

    bool flip   = m_Vsync && m_DoubleBuffered;
    u16 *target = flip ? m_pBack : m_pFront;

    if (width != m_LastW || height != m_LastH)
    {
        ClearBlack();                 // both pages: repaint letterbox/pillarbox
        m_LastW = width;
        m_LastH = height;
    }

    if (m_ScaleMode == ScaleMode::Stretch && width != 0 && height != 0)
    {
        // Largest 4:3 rectangle that fits the framebuffer, centered; the frame
        // is stretched (non-integer) to fill it.
        unsigned rw = m_FbW;
        unsigned rh = m_FbW * 3 / 4;
        if (rh > m_FbH) { rh = m_FbH; rw = m_FbH * 4 / 3; }
        unsigned ox = (m_FbW - rw) / 2;
        unsigned oy = (m_FbH - rh) / 2;
        blit_rgb565_scaled(target, m_Pitch, m_FbW, m_FbH,
                           (const uint16_t *) src, (unsigned) pitch,
                           width, height, ox, oy, rw, rh);
        if (flip) Present();
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

    blit_rgb565(target, m_Pitch, m_FbW, m_FbH,
                (const uint16_t *) src, (unsigned) pitch, width, height, scale);
    if (flip) Present();
}
```

- [ ] **Step 5: Build the kernel to verify it compiles and links**

Run: `make`
Expected: SUCCESS — builds `kernel7.img` with no errors.

- [ ] **Step 6: Commit**

```bash
git add src/video/display.h src/video/display.cpp
git commit -m "Video: double-buffer with vsync page flip (tear-free); single-buffer fallback

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Wire the setting + Settings row + hardware checklist

Applies `vsync` at boot and live from the Settings screen, adds the "Vsync:" row,
and records hardware-verification steps. Completes the feature.

**Files:**
- Modify: `src/kernel.cpp:125`
- Modify: `src/menu/settings_screen.cpp`
- Modify: `docs/hardware-verification-checklist-2026-06-20.md`

**Interfaces:**
- Consumes: `Settings::vsync` (Task 1); `Display::SetVsync(bool)` (Task 2).
- Produces: nothing other tasks depend on.

- [ ] **Step 1: Apply vsync at boot**

In `src/kernel.cpp`, after line 125 (`m_Display.SetScaleMode (m_Settings.scale_mode);`)
add:

```cpp
	m_Display.SetVsync (m_Settings.vsync);
```

- [ ] **Step 2: Apply vsync live from the Settings screen**

In `src/menu/settings_screen.cpp`, in `Apply()`, after the `SetScaleMode` line
(line 38) add:

```cpp
    m_pDisplay->SetVsync(m_pSettings->vsync);            // live
```

- [ ] **Step 3: Grow the row count**

In `src/menu/settings_screen.cpp`, change line 16:

```cpp
#define NUM_ROWS 11
```

- [ ] **Step 4: Add the value string and the row**

In `Render()`, add the value string after the `audioVal` block (after line 87):

```cpp
    const char *vsyncVal = m_pSettings->vsync ? "< On >" : "< Off >";
```

Replace the `labels` and `values` arrays (lines 88-95) — "Vsync:" is inserted
after "Audio out:", before the two submenu rows:

```cpp
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Audio out:",
                                     "Vsync:",
                                     "Controls...", "Video Mode..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, audioVal,
                                     vsyncVal, "", "" };
```

- [ ] **Step 5: Add the toggle handler and fix the submenu indices**

In `Run()`, add a new `case 8` to the left/right `switch (selected)` block,
immediately after the Audio out `case 7` block (after line 203):

```cpp
            case 8:   // Vsync (toggle tear-free page flip; live)
                m_pSettings->vsync = !m_pSettings->vsync;
                break;
```

The two submenu rows shifted down by one. In the `GP_START` handler, change
`selected == 8` to `selected == 9` (Controls...) and `selected == 9` to
`selected == 10` (Video Mode...):

```cpp
        if (pressed & GP_START)
        {
            if (selected == 9)                        // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 10)                  // Video Mode...
            {
                m_pVideoMode->Run();
```

- [ ] **Step 6: Build to verify it compiles**

Run: `make`
Expected: SUCCESS — builds `kernel7.img` with no errors.

- [ ] **Step 7: Add the hardware-verification entry**

In `docs/hardware-verification-checklist-2026-06-20.md`, add a new section
immediately before the `## Results summary` line:

```markdown
## K. Tear-free output (vsync)

- [ ] **K1 — No tearing.** Play a horizontally-scrolling game (e.g. Sonic).
  **Expect:** no tear line across the screen during scroll (vsync defaults on).
- [ ] **K2 — Toggle proves the path.** Pause → Settings → **Vsync** = Off, resume.
  **Expect:** tearing returns (off == old single-buffer path). Set back to On →
  tearing gone. (Live, no reboot.)
- [ ] **K3 — No pacing regression.** With Vsync on, confirm the game runs full
  speed and the periodic underrun/overrun log doesn't climb vs. Vsync off.
- [ ] **K4 — Menus clean.** Open/close the pause menu several times during play.
  **Expect:** no menu remnants or letterbox-bar artifacts after resuming.
- [ ] **K5 — Persisted.** Confirm `vsync=on` (or `off`) in `SD:/settings.txt`;
  survives reboot.

---
```

Also add the row to the results-summary table (after the `| J. Analog audio | | |`
row):

```markdown
| K. Tear-free (vsync) | | |
```

- [ ] **Step 8: Commit**

```bash
git add src/kernel.cpp src/menu/settings_screen.cpp docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Settings: Vsync row + apply at boot/live + hardware-check entry

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes / deviations from the spec

- **`vsync` parse uses the shared `truthy()`** (like `mute`/`widescreen`): a
  missing key yields the default (`true`); an explicit non-truthy value (e.g.
  `off`) reads `false`. This is the codebase convention; the spec's
  "unknown/missing → default" is satisfied for missing keys.
- **`ForceRepaint()` keeps its one-line `m_Last` reset** rather than eagerly
  memset-ing both pages: the next `Blit` hits the size-change branch, which now
  calls `ClearBlack()` (both pages), so menu/bar remnants are wiped before any
  flip — same effect as the spec describes, less code.
