# In-Game HUD / Overlay Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-skin the in-game diagnostics HUD and toast notices to the v1.0.0 GlyphCanvas look with semantic health coloring, expanded state fields, and colored toast pills.

**Architecture:** Three layers. (1) Pure host-tested `hud` layer gains a cell-based `hud_build()` with health classification, replacing flat-line `hud_format()`. (2) `Overlay` retargets from `TextCanvas` to `GlyphCanvas`, drawing a translucent panel + scanlines with per-cell health colors and translucent toast pills. (3) `kernel.cpp` populates the new fields and passes a toast kind at each call site.

**Tech Stack:** C++ (bare-metal, no STL/snprintf), Circle `GlyphCanvas`/`Display`, host unit tests via `test/Makefile` (system `c++`).

## Global Constraints

- **No `snprintf` / no STL** in `src/` code that links bare-metal — use manual digit formatting (see `app_uint` in `hud.cpp`).
- **No changes to `Display::Blit` / `Display::Present`** — the tuned vsync/pacing path must stay untouched. The Overlay draws after `retro_run()` on the front page only.
- **Full fixed-size repaint each frame** for HUD panel and toast pill (no ghosting across the two vsync pages).
- **Theme colors only** (`src/ui/theme.h`), never raw hex.
- **Does NOT retire `TextCanvas`** — `splash_draw.cpp` still uses it. Leave `m_Canvas` in `kernel.h`.
- Branch: `feat/hud-redesign` (already created, holds the spec).
- Host tests run from `test/`: `make run`.

---

### Task 1: Pure layer — `hud_build`, health classification, expanded `HudStats`

**Files:**
- Modify: `src/ui/hud.h`
- Modify: `src/ui/hud.cpp`
- Test: `test/test_hud.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `enum HudHealth { HUD_GOOD, HUD_WARN, HUD_BAD, HUD_INFO };`
  - `struct HudCell { char label[HUD_LABEL_MAX+1]; char value[HUD_VALUE_MAX+1]; HudHealth health; };`
  - `unsigned hud_build(const HudStats &s, HudCell cells[], unsigned max);` — fills up to `max` cells in fixed order (FPS, AQ, UR, OR, ROM, MODE, VSYNC, WIDE, LAT), returns count.
  - `HudStats` gains `bool vsync; bool widescreen; const char *latency;`.
  - `HUD_LABEL_MAX` (6), `HUD_VALUE_MAX` (18), `HUD_CELL_MAX` (9).
  - `hud_format()` is KEPT in this task (removed in Task 2) so the kernel still builds.

- [ ] **Step 1: Write the failing test** — replace `test/test_hud.cpp` with:

```cpp
#include "../src/ui/hud.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static const HudCell *find(const HudCell *c, unsigned n, const char *label)
{
    for (unsigned i = 0; i < n; i++) if (strcmp(c[i].label, label) == 0) return &c[i];
    return 0;
}

int main(void)
{
    HudCell cells[HUD_CELL_MAX];

    HudStats s;
    s.fps = 60; s.underruns = 0; s.overruns = 2;
    s.queued = 1440; s.target = 2880;
    s.rom = "SD:/roms/Sonic.bin"; s.mode = "1080p"; s.scale = "aspect";
    s.vsync = false; s.widescreen = true; s.latency = "medium";

    unsigned n = hud_build(s, cells, HUD_CELL_MAX);
    assert(n == 9);

    // FPS healthy at 60.
    const HudCell *fps = find(cells, n, "FPS");
    assert(fps && strcmp(fps->value, "60") == 0 && fps->health == HUD_GOOD);

    // Underruns 0 -> good; overruns 2 -> bad.
    assert(find(cells, n, "UR")->health == HUD_GOOD);
    assert(strcmp(find(cells, n, "UR")->value, "0") == 0);
    assert(find(cells, n, "OR")->health == HUD_BAD);
    assert(strcmp(find(cells, n, "OR")->value, "2") == 0);

    // AQ queued/target, info.
    assert(strcmp(find(cells, n, "AQ")->value, "1440/2880") == 0);
    assert(find(cells, n, "AQ")->health == HUD_INFO);

    // ROM dir stripped.
    assert(strcmp(find(cells, n, "ROM")->value, "Sonic.bin") == 0);

    // MODE = mode + space + scale.
    assert(strcmp(find(cells, n, "MODE")->value, "1080p aspect") == 0);

    // New state fields.
    assert(strcmp(find(cells, n, "VSYNC")->value, "off") == 0);
    assert(strcmp(find(cells, n, "WIDE")->value, "on") == 0);
    assert(strcmp(find(cells, n, "LAT")->value, "medium") == 0);

    // FPS health thresholds: 58 good, 57 warn, 50 warn, 49 bad.
    HudStats h = s;
    h.fps = 58; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_GOOD);
    h.fps = 57; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_WARN);
    h.fps = 50; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_WARN);
    h.fps = 49; hud_build(h, cells, HUD_CELL_MAX); assert(find(cells,9,"FPS")->health == HUD_BAD);

    // Long ROM name truncated to HUD_VALUE_MAX, dir stripped.
    HudStats l = s;
    l.rom = "SD:/roms/A Very Long Game Name That Exceeds.bin";
    hud_build(l, cells, HUD_CELL_MAX);
    assert(strlen(find(cells, 9, "ROM")->value) == HUD_VALUE_MAX);

    // NULL rom -> "-"; NULL mode/scale safe.
    HudStats z; z.fps = 0; z.underruns = 0; z.overruns = 0;
    z.queued = 0; z.target = 0; z.rom = 0; z.mode = 0; z.scale = 0;
    z.vsync = true; z.widescreen = false; z.latency = "low";
    unsigned zn = hud_build(z, cells, HUD_CELL_MAX);
    assert(zn == 9);
    assert(strcmp(find(cells, zn, "ROM")->value, "-") == 0);
    assert(strcmp(find(cells, zn, "VSYNC")->value, "on") == 0);

    // Bounds: every label/value within caps.
    for (unsigned i = 0; i < zn; i++) {
        assert(strlen(cells[i].label) <= HUD_LABEL_MAX);
        assert(strlen(cells[i].value) <= HUD_VALUE_MAX);
    }

    // Respects max.
    assert(hud_build(s, cells, 3) == 3);

    printf("All hud tests passed\n");
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_hud`
Expected: FAIL — compile error (`HudCell` / `hud_build` / `HUD_CELL_MAX` not declared).

- [ ] **Step 3: Extend `src/ui/hud.h`** — add the new constants, enum, struct, `HudStats` fields, and `hud_build` declaration, keeping `hud_format` for now:

```c
#ifndef _ui_hud_h
#define _ui_hud_h

#include <stddef.h>

#define HUD_COLS  22
#define HUD_LINES 4

#define HUD_LABEL_MAX 6
#define HUD_VALUE_MAX 18
#define HUD_CELL_MAX  9

struct HudStats
{
    unsigned    fps;
    unsigned    underruns;
    unsigned    overruns;
    unsigned    queued;
    unsigned    target;
    const char *rom;
    const char *mode;
    const char *scale;
    bool        vsync;       // tear-free page flip on/off
    bool        widescreen;  // widescreen on/off
    const char *latency;     // "low" | "medium" | "high"
};

enum HudHealth { HUD_GOOD, HUD_WARN, HUD_BAD, HUD_INFO };

struct HudCell
{
    char      label[HUD_LABEL_MAX + 1];
    char      value[HUD_VALUE_MAX + 1];
    HudHealth health;
};

// Fill up to max cells (fixed order: FPS, AQ, UR, OR, ROM, MODE, VSYNC, WIDE,
// LAT). Returns the number written.
unsigned hud_build(const HudStats &s, HudCell cells[], unsigned max);

// Legacy flat-line formatter (removed once Overlay no longer uses it).
unsigned hud_format(const HudStats &s, char lines[][HUD_COLS + 1],
                    unsigned max_lines);

#endif
```

- [ ] **Step 4: Implement `hud_build` in `src/ui/hud.cpp`** — add above the existing `hud_format` (leave `hud_format` and its `app_*`/`base_name` helpers in place):

```c
// --- cell builder helpers (bounded, NUL-terminated, no snprintf) ---

static void cell_uint(char *dst, unsigned v, unsigned cap)
{
    char tmp[10];
    int  n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char) ('0' + v % 10); v /= 10; }
    unsigned i = 0;
    while (n > 0 && i < cap) dst[i++] = tmp[--n];
    dst[i] = '\0';
}

static void cell_str(char *dst, const char *src, unsigned cap)
{
    unsigned i = 0;
    if (src) for (; src[i] && i < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
}

// Append (no overflow past cap); *len tracks current length.
static void cell_append(char *dst, unsigned *len, const char *src, unsigned cap)
{
    if (src) while (*src && *len < cap) dst[(*len)++] = *src++;
    dst[*len] = '\0';
}
static void cell_append_uint(char *dst, unsigned *len, unsigned v, unsigned cap)
{
    char tmp[10];
    int  n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char) ('0' + v % 10); v /= 10; }
    while (n > 0 && *len < cap) dst[(*len)++] = tmp[--n];
    dst[*len] = '\0';
}

static HudHealth fps_health(unsigned fps)
{
    if (fps >= 58) return HUD_GOOD;
    if (fps >= 50) return HUD_WARN;
    return HUD_BAD;
}
static HudHealth count_health(unsigned n) { return n == 0 ? HUD_GOOD : HUD_BAD; }

static void set_cell(HudCell *c, const char *label, HudHealth h)
{
    cell_str(c->label, label, HUD_LABEL_MAX);
    c->value[0] = '\0';
    c->health   = h;
}

unsigned hud_build(const HudStats &s, HudCell cells[], unsigned max)
{
    unsigned k = 0;
    unsigned len;

    if (k >= max) return k;                                  // FPS
    set_cell(&cells[k], "FPS", fps_health(s.fps));
    cell_uint(cells[k].value, s.fps, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // AQ queued/target
    set_cell(&cells[k], "AQ", HUD_INFO);
    len = 0;
    cell_append_uint(cells[k].value, &len, s.queued, HUD_VALUE_MAX);
    cell_append(cells[k].value, &len, "/", HUD_VALUE_MAX);
    cell_append_uint(cells[k].value, &len, s.target, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // UR
    set_cell(&cells[k], "UR", count_health(s.underruns));
    cell_uint(cells[k].value, s.underruns, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // OR
    set_cell(&cells[k], "OR", count_health(s.overruns));
    cell_uint(cells[k].value, s.overruns, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // ROM base name
    set_cell(&cells[k], "ROM", HUD_INFO);
    cell_str(cells[k].value, base_name(s.rom), HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // MODE = mode scale
    set_cell(&cells[k], "MODE", HUD_INFO);
    len = 0;
    if (s.mode) cell_append(cells[k].value, &len, s.mode, HUD_VALUE_MAX);
    cell_append(cells[k].value, &len, " ", HUD_VALUE_MAX);
    if (s.scale) cell_append(cells[k].value, &len, s.scale, HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // VSYNC
    set_cell(&cells[k], "VSYNC", HUD_INFO);
    cell_str(cells[k].value, s.vsync ? "on" : "off", HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // WIDE
    set_cell(&cells[k], "WIDE", HUD_INFO);
    cell_str(cells[k].value, s.widescreen ? "on" : "off", HUD_VALUE_MAX);
    k++;

    if (k >= max) return k;                                  // LAT
    set_cell(&cells[k], "LAT", HUD_INFO);
    cell_str(cells[k].value, s.latency ? s.latency : "-", HUD_VALUE_MAX);
    k++;

    return k;
}
```

Note: `base_name` is the existing `static` helper already in `hud.cpp` (used by `hud_format`); reuse it. If the compiler warns it's defined after first use, move `hud_build` below `hud_format` in the file.

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd test && make test_hud && ./test_hud`
Expected: `All hud tests passed`

- [ ] **Step 6: Run the full host suite** (no regressions in callers that include `hud.h`)

Run: `cd test && make run`
Expected: every line prints its `All ... passed`; no compile errors.

- [ ] **Step 7: Commit**

```bash
git add src/ui/hud.h src/ui/hud.cpp test/test_hud.cpp
git commit -m "feat(hud): cell-based hud_build with health + expanded state fields

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Renderer + kernel wiring — `GlyphCanvas` HUD, toast pills, retire `hud_format`

**Files:**
- Modify: `src/ui/overlay.h`
- Modify: `src/ui/overlay.cpp`
- Modify: `src/kernel.cpp` (lines ~54, ~342–378, ~404–413)
- Modify: `src/ui/hud.h`, `src/ui/hud.cpp` (remove `hud_format`)
- Test: `test/test_hud.cpp` (remove leftover `hud_format` include usage — already replaced in Task 1, no change needed)

**Interfaces:**
- Consumes: `hud_build`, `HudCell`, `HudHealth` (Task 1); `GlyphCanvas` (`BlendRect`, `Scanlines`, `Text`, `TextWidth`, `Width`, `Height`); `g_font_vt323_16`; `theme::*`.
- Produces:
  - `enum ToastKind { TOAST_INFO, TOAST_SUCCESS, TOAST_FAIL };`
  - `Overlay(GlyphCanvas *pCanvas);`
  - `void Overlay::ShowToast(const char *msg, ToastKind kind = TOAST_INFO);`
  - (unchanged signatures) `SetEnabled`, `Enabled`, `Draw(const HudStats&)`, `DrawToast()`.

- [ ] **Step 1: Rewrite `src/ui/overlay.h`**

```c
//
// src/ui/overlay.h
//
// Bare Metal Sega Genesis
// Composites the diagnostics HUD + toasts onto the game framebuffer via
// GlyphCanvas (v1.0.0 look). Drawn after retro_run() each frame; no-op when
// disabled. Does not touch Display::Blit/Present.
//

#ifndef _ui_overlay_h
#define _ui_overlay_h

#include "glyph_canvas.h"
#include "hud.h"

#define TOAST_MAX    28
#define TOAST_FRAMES 120   // ~2 s at 60 fps

enum ToastKind { TOAST_INFO, TOAST_SUCCESS, TOAST_FAIL };

class Overlay
{
public:
    explicit Overlay(GlyphCanvas *pCanvas);

    void SetEnabled(bool on) { m_Enabled = on; }
    bool Enabled(void) const { return m_Enabled; }

    void Draw(const HudStats &s);   // no-op if disabled

    void ShowToast(const char *msg, ToastKind kind = TOAST_INFO);
    void DrawToast(void);

private:
    GlyphCanvas *m_pCanvas;
    bool         m_Enabled;
    char         m_Toast[TOAST_MAX + 1];
    ToastKind    m_ToastKind;
    unsigned     m_ToastFrames;
};

#endif
```

- [ ] **Step 2: Rewrite `src/ui/overlay.cpp`**

```c
//
// src/ui/overlay.cpp
//
// Bare Metal Sega Genesis
// See overlay.h.
//

#include "overlay.h"
#include "theme.h"
#include "fonts/font_vt323_16.h"

// Panel/pill geometry (tuned on hardware if needed).
#define HUD_PAD   8
#define HUD_LH    18    // line height for VT323-16
#define HUD_COLW  108   // column width (two columns -> ~216 px panel)
#define HUD_X     8
#define HUD_Y     8

// Two-column layout for the 9 cells from hud_build (in fixed order).
struct Slot { int row; int col; };
static const Slot kLayout[HUD_CELL_MAX] = {
    {0,0},{0,1},   // FPS | AQ
    {1,0},{1,1},   // UR  | OR
    {2,0},         // ROM  (full width: starts col 0, no right neighbor)
    {3,0},         // MODE
    {4,0},{4,1},   // VSYNC | WIDE
    {5,0},         // LAT
};
static const int kRows = 6;

static u16 health_color(HudHealth h)
{
    switch (h)
    {
    case HUD_GOOD: return theme::ACTIVE;     // green
    case HUD_WARN: return theme::ADJUST;     // amber
    case HUD_BAD:  return theme::SELECTION;  // red
    default:       return theme::VALUE;      // cyan (info)
    }
}

static u16 toast_color(ToastKind k)
{
    switch (k)
    {
    case TOAST_SUCCESS: return theme::ACTIVE;
    case TOAST_FAIL:    return theme::SELECTION;
    default:            return theme::VALUE;
    }
}

Overlay::Overlay(GlyphCanvas *pCanvas)
:   m_pCanvas(pCanvas), m_Enabled(false), m_ToastKind(TOAST_INFO), m_ToastFrames(0)
{
    m_Toast[0] = '\0';
}

void Overlay::Draw(const HudStats &s)
{
    if (!m_Enabled) return;

    HudCell cells[HUD_CELL_MAX];
    unsigned n = hud_build(s, cells, HUD_CELL_MAX);

    const Font *f = &g_font_vt323_16;
    int panelW = HUD_PAD * 2 + HUD_COLW * 2;
    int panelH = HUD_PAD * 2 + kRows * HUD_LH;

    // Translucent panel + scanlines (full fixed-size repaint each frame).
    m_pCanvas->BlendRect(HUD_X, HUD_Y, panelW, panelH, theme::BG, 190);
    m_pCanvas->Scanlines(HUD_X, HUD_Y, panelW, panelH, 60);

    for (unsigned i = 0; i < n; i++)
    {
        int x = HUD_X + HUD_PAD + kLayout[i].col * HUD_COLW;
        int y = HUD_Y + HUD_PAD + kLayout[i].row * HUD_LH;
        // label (muted) then value (health color), flowing left-to-right.
        int vx = m_pCanvas->Text(f, 1, x, y, cells[i].label,
                                 theme::TEXT_MUTED, 0, true);
        vx += f->width;  // one-char gap
        m_pCanvas->Text(f, 1, vx, y, cells[i].value,
                        health_color(cells[i].health), 0, true);
    }
}

void Overlay::ShowToast(const char *msg, ToastKind kind)
{
    unsigned i = 0;
    if (msg != 0)
        for (; msg[i] != '\0' && i < TOAST_MAX; i++) m_Toast[i] = msg[i];
    m_Toast[i]    = '\0';
    m_ToastKind   = kind;
    m_ToastFrames = TOAST_FRAMES;
}

void Overlay::DrawToast(void)
{
    if (m_ToastFrames == 0) return;
    m_ToastFrames--;

    const Font *f = &g_font_vt323_16;
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();

    int padX = 12, padY = 6;
    int textW = m_pCanvas->TextWidth(f, 1, m_Toast);
    int boxW  = textW + 2 * padX;
    int boxH  = (int) f->height + 2 * padY;
    int x = (W - boxW) / 2; if (x < 0) x = 0;
    int y = H - boxH - 36;  // near bottom (HUD is top-left)

    // Translucent pill (full repaint each frame).
    m_pCanvas->BlendRect(x, y, boxW, boxH, theme::BG, 200);
    m_pCanvas->Scanlines(x, y, boxW, boxH, 60);
    m_pCanvas->Text(f, 1, x + padX, y + padY, m_Toast,
                    toast_color(m_ToastKind), 0, true);
}
```

Note: `Font` has fields `width`/`height` (see `src/ui/font.h`). If `GlyphCanvas::Text`'s signature differs from `Text(const Font*, int scale, int x, int y, const char*, u16 fg, u16 bg, bool transparent)`, match the actual header (already confirmed in `glyph_canvas.h`).

- [ ] **Step 3: Wire the kernel — construct with `&m_GlyphCanvas`** (`src/kernel.cpp` ~line 54)

Change:
```cpp
	m_Overlay (&m_Canvas),
```
to:
```cpp
	m_Overlay (&m_GlyphCanvas),
```

- [ ] **Step 4: Populate new `HudStats` fields** (`src/kernel.cpp`, in the `if (m_Overlay.Enabled ())` block, after `st.scale = ...`)

Add:
```cpp
				st.vsync      = m_Settings.vsync;
				st.widescreen = m_Settings.widescreen;
				st.latency    = audio_latency_file_value (m_Settings.audio_latency);
```

- [ ] **Step 5: Pass `ToastKind` at the call sites** (`src/kernel.cpp` ~342–378)

QuickSave:
```cpp
			case InGameAction::QuickSave:
			{
				bool ok = m_SaveState.Save (1);
				m_Overlay.ShowToast (ok ? "Quick-saved" : "Save failed",
				                     ok ? TOAST_SUCCESS : TOAST_FAIL);
				break;
			}
```

QuickLoad:
```cpp
			case InGameAction::QuickLoad:
				if (!m_SaveState.Occupied (1))
					m_Overlay.ShowToast ("No quick save", TOAST_FAIL);
				else
				{
					bool ok = m_SaveState.Load (1);
					m_Overlay.ShowToast (ok ? "Quick-loaded" : "Load failed",
					                     ok ? TOAST_SUCCESS : TOAST_FAIL);
				}
				break;
```

Volume toast (line ~364) — info kind (explicit, though it's the default):
```cpp
				m_Overlay.ShowToast (t, TOAST_INFO);
```

HUD toggle (line ~371):
```cpp
				m_Overlay.ShowToast (m_Settings.debug_overlay ? "HUD on" : "HUD off",
				                     TOAST_INFO);
```

Mute (line ~378):
```cpp
				m_Overlay.ShowToast (m_Settings.mute ? "Muted" : "Unmuted", TOAST_INFO);
```

- [ ] **Step 6: Remove the now-dead `hud_format`** — delete its declaration in `src/ui/hud.h` (the block under "Legacy flat-line formatter") and its definition + the `app_uint`/`app_str` helpers in `src/ui/hud.cpp`. Keep `base_name` (used by `hud_build`). Confirm nothing else references `hud_format`:

Run: `grep -rn "hud_format" src test`
Expected: no matches.

- [ ] **Step 7: Build the kernel (cross toolchain)**

Run: `make 2>&1 | tail -20`
Expected: `LD kernel7.elf` ... `COPY kernel7.img`, EXIT 0, no warnings from `overlay.cpp`/`hud.cpp`/`kernel.cpp`.

- [ ] **Step 8: Run the host suite** (hud test still green after `hud_format` removal)

Run: `cd test && make run`
Expected: all suites pass.

- [ ] **Step 9: Commit**

```bash
git add src/ui/overlay.h src/ui/overlay.cpp src/ui/hud.h src/ui/hud.cpp src/kernel.cpp
git commit -m "feat(hud): re-skin Overlay to GlyphCanvas with health colors + toast pills

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Hardware-verify checklist + memory note

**Files:**
- Modify: most recent `docs/hardware-verification-checklist-*.md` (add a HUD-redesign section)
- Create: memory note (see Step 2)

- [ ] **Step 1: Add a checklist section.** Find the current checklist file:

Run: `ls docs/hardware-verification-checklist-*.md`

Append a new section to the newest one:

```markdown
## Section V — HUD / Overlay redesign (feat/hud-redesign)

- [ ] V1. Enable Debug Overlay (settings) — HUD shows as a translucent panel with
       scanlines, top-left, ~6 rows; gameplay faintly visible behind it.
- [ ] V2. FPS reads green at ~60; force a slowdown and confirm amber (~50s) / red (<50).
- [ ] V3. Force an audio underrun (e.g. heavy scene) — UR/OR turn red; otherwise green.
- [ ] V4. New rows present and correct: VSYNC on/off, WIDE on/off, LAT low/medium/high
       match the settings.
- [ ] V5. Quick-save shows a green pill ("Quick-saved"); quick-load with no save shows
       a red pill ("No quick save").
- [ ] V6. Volume / HUD-toggle / mute toasts show as neutral (cyan) pills, bottom-center,
       fading after ~2 s.
- [ ] V7. No FPS/pacing regression with HUD on vs off (read the FPS row).
- [ ] V8. Toggling HUD off cleanly removes the panel (ForceRepaint), no ghost pixels.
```

- [ ] **Step 2: Write the memory note** to `/home/matt/.claude/projects/-home-matt-Bare-Metal-Sega-Genesis/memory/project_hud_redesign.md`:

```markdown
---
name: project_hud_redesign
description: HUD/Overlay re-skinned to GlyphCanvas (translucent panel + health colors + toast pills); code-complete on branch feat/hud-redesign, PENDING HARDWARE VERIFY (checklist V)
metadata:
  type: project
---

Final piece of the on-screen UI redesign ([[project_ui_redesign]]): the in-game
diagnostics HUD + toasts (the last UI on the legacy TextCanvas) re-skinned to the
v1.0.0 GlyphCanvas look. Branch `feat/hud-redesign`. Spec
`docs/superpowers/specs/2026-06-22-hud-redesign-design.md`, plan
`docs/superpowers/plans/2026-06-22-hud-redesign.md`.

What shipped: pure cell-based `hud_build()` + `HudHealth` (replaces flat-line
`hud_format`), expanded `HudStats` (vsync/widescreen/latency). `Overlay` retargeted
to GlyphCanvas: translucent panel + scanlines, per-value semantic health colors
(FPS green/amber/red, UR/OR red when >0), and translucent toast pills colored by
ToastKind (success green / fail red / info cyan). Drawn after retro_run on the
front page — Display::Blit/Present untouched ([[project_tear_free_output]]).

Does NOT retire TextCanvas: splash.raw image blit in splash_draw.cpp still uses it
(GlyphCanvas has no image primitive). Full retirement is a separate follow-up.

PENDING HARDWARE VERIFY — checklist section V. Builds clean (host suite + kernel).
Relates to [[project_overlay_hud]], [[project_ingame_hotkeys_toasts]].
```

- [ ] **Step 3: Add the index line** to `MEMORY.md` (top of the list), then commit:

```bash
git add docs/hardware-verification-checklist-*.md
git commit -m "docs(hud): hardware-verify checklist section V for HUD redesign

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

(The memory files live outside the repo; write them but do not `git add` them.)

---

## Notes for the implementer

- The renderer (`overlay.cpp`) has no host test — it's Circle-side. Its verification is the clean kernel build (Task 2 Step 7) plus the hardware checklist (Task 3). This matches the existing pattern for `glyph_canvas`/`text_canvas`.
- Pixel offsets (`HUD_COLW`, `HUD_LH`, padding) are first-pass values; expect to nudge them on hardware. They do not affect correctness, only layout.
- If `make` reports `KERNEL_MAX_SIZE`/boot-size concerns, the image only grows by a few KB here; no action expected.
