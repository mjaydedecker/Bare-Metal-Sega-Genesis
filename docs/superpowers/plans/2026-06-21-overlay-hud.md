# On-Screen Overlay Layer + Diagnostics HUD Implementation Plan (Spec A)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a toggleable on-screen diagnostics HUD (measured FPS, audio underruns/overruns, audio queue depth, ROM + video mode/scale) composited over the live game frame, gated by a persisted `debug_overlay` setting.

**Architecture:** A pure host-tested `hud_format()` (`src/ui/hud.*`) turns a `HudStats` struct into fixed-width text lines; a thin device `Overlay` class (`src/ui/overlay.*`) draws those lines via the existing `TextCanvas`. The kernel measures FPS, builds `HudStats` each frame, and calls `Overlay::Draw()` after `retro_run()` on the visible front page — touching none of the vsync/pacing hot path.

**Tech Stack:** C++ bare-metal Circle (device) + host C++ unit tests via `test/Makefile`. RGB565 framebuffer; `CCharGenerator` font through `TextCanvas`.

## Global Constraints

- `src/ui/hud.*` is **pure** (host-testable): include only `<stddef.h>`; no Circle, no `snprintf`/heap. Format numbers manually (mirror `fmt_volume` in `settings_screen.cpp`).
- `HUD_COLS = 22`, `HUD_LINES = 4`. Every produced line is NUL-terminated and at most `HUD_COLS` characters.
- New kernel objects must be added to `OBJS` in the root `Makefile` (explicit list; no wildcard).
- `debug_overlay` defaults to **false**; the HUD is opt-in. No change to `Display::Blit`/`Present`.
- Follow existing setting patterns exactly (parse via `truthy()`, serialize `on`/`off`).
- Host tests run with `make -C test run`; the device build is `make` from repo root.

---

### Task 1: `hud_format()` pure module + host test

**Files:**
- Create: `src/ui/hud.h`, `src/ui/hud.cpp`
- Create: `test/test_hud.cpp`
- Modify: `test/Makefile` (add `test_hud` target + to `run`)
- Modify: `Makefile` (add `src/ui/hud.o` to `OBJS`)

**Interfaces:**
- Consumes: nothing.
- Produces: `struct HudStats`; `unsigned hud_format(const HudStats &s, char lines[][HUD_COLS + 1], unsigned max_lines);` — fills up to `max_lines` lines, returns count. Line 0 `FPS f  U u  O o`; line 1 `AQ q/t`; line 2 ROM base name (dir stripped, truncated); line 3 `mode  scale`. NULL `rom` → `"-"`; NULL `mode`/`scale` → omitted.

- [ ] **Step 1: Write `src/ui/hud.h`**

```cpp
//
// src/ui/hud.h
//
// Bare Metal Sega Genesis
// Pure formatter for the diagnostics HUD. Turns runtime stats into fixed-width
// text lines. No Circle dependency — host-testable.
//

#ifndef _ui_hud_h
#define _ui_hud_h

#include <stddef.h>

#define HUD_COLS  22
#define HUD_LINES 4

struct HudStats
{
    unsigned    fps;        // measured frames/sec
    unsigned    underruns;  // audio underrun count
    unsigned    overruns;   // audio overrun count
    unsigned    queued;     // audio frames currently queued
    unsigned    target;     // pacing target queue depth
    const char *rom;        // ROM path or name (may be NULL); dir is stripped
    const char *mode;       // "native"/"1080p"/... (may be NULL)
    const char *scale;      // "integer"/"stretch"/"aspect" (may be NULL)
};

// Fill up to max_lines NUL-terminated lines (each <= HUD_COLS chars). Returns
// the number of lines written.
unsigned hud_format(const HudStats &s, char lines[][HUD_COLS + 1],
                    unsigned max_lines);

#endif
```

- [ ] **Step 2: Write the failing test `test/test_hud.cpp`**

```cpp
#include "../src/ui/hud.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    char lines[HUD_LINES][HUD_COLS + 1];

    HudStats s;
    s.fps = 59; s.underruns = 0; s.overruns = 2;
    s.queued = 1440; s.target = 2880;
    s.rom = "SD:/roms/Sonic.bin";
    s.mode = "1080p"; s.scale = "aspect";

    unsigned n = hud_format(s, lines, HUD_LINES);
    assert(n == 4);
    assert(strcmp(lines[0], "FPS 59  U 0  O 2") == 0);
    assert(strcmp(lines[1], "AQ 1440/2880") == 0);
    assert(strcmp(lines[2], "Sonic.bin") == 0);   // dir stripped
    assert(strcmp(lines[3], "1080p  aspect") == 0);
    for (unsigned i = 0; i < n; i++) assert(strlen(lines[i]) <= HUD_COLS);

    // Long ROM name is truncated to HUD_COLS, dir stripped.
    HudStats l = s;
    l.rom = "SD:/roms/A Very Long Game Name That Exceeds.bin";
    hud_format(l, lines, HUD_LINES);
    assert(strlen(lines[2]) == HUD_COLS);
    assert(strncmp(lines[2], "A Very Long Game Name", 21) == 0);

    // NULL rom -> "-"; NULL mode/scale safe.
    HudStats z; z.fps = 0; z.underruns = 0; z.overruns = 0;
    z.queued = 0; z.target = 0; z.rom = 0; z.mode = 0; z.scale = 0;
    unsigned zn = hud_format(z, lines, HUD_LINES);
    assert(zn == 4);
    assert(strcmp(lines[0], "FPS 0  U 0  O 0") == 0);
    assert(strcmp(lines[1], "AQ 0/0") == 0);
    assert(strcmp(lines[2], "-") == 0);
    for (unsigned i = 0; i < zn; i++) assert(strlen(lines[i]) <= HUD_COLS);

    // Respects max_lines.
    assert(hud_format(s, lines, 2) == 2);

    printf("All hud tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Add the `test_hud` target to `test/Makefile`**

Add `test_hud` to the `run:` dependency list and its run line, and add the rule:

```make
test_hud: test_hud.cpp ../src/ui/hud.cpp ../src/ui/hud.h
	$(CXX) $(CXXFLAGS) -o $@ test_hud.cpp ../src/ui/hud.cpp
```

Also add `test_hud` to the `run:` target's prerequisites and add `./test_hud` to its recipe, and add `test_hud` to the `clean:` `rm -f` list.

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C test test_hud`
Expected: FAIL — linker/compile error, `hud_format` undefined (no `hud.cpp` yet).

- [ ] **Step 5: Write `src/ui/hud.cpp`**

```cpp
//
// src/ui/hud.cpp
//
// Bare Metal Sega Genesis
// See hud.h. Manual formatting only (no snprintf) so it links on bare metal.
//

#include "hud.h"

// Append decimal v to line[*len], bounded by HUD_COLS. Keeps line NUL-terminated.
static void app_uint(char *line, unsigned *len, unsigned v)
{
    char tmp[10];
    int  n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char) ('0' + v % 10); v /= 10; }
    while (n > 0 && *len < HUD_COLS) line[(*len)++] = tmp[--n];
    line[*len] = '\0';
}

// Append string src to line[*len], bounded by HUD_COLS.
static void app_str(char *line, unsigned *len, const char *src)
{
    while (*src && *len < HUD_COLS) line[(*len)++] = *src++;
    line[*len] = '\0';
}

// Pointer to the char after the last '/' or ':' in p ("-" if p is NULL).
static const char *base_name(const char *p)
{
    if (p == 0) return "-";
    const char *b = p;
    for (const char *q = p; *q; q++)
        if (*q == '/' || *q == ':') b = q + 1;
    return b;
}

unsigned hud_format(const HudStats &s, char lines[][HUD_COLS + 1],
                    unsigned max_lines)
{
    unsigned count = 0;
    unsigned len;

    if (count >= max_lines) return count;          // line 0: FPS / U / O
    len = 0; lines[count][0] = '\0';
    app_str(lines[count], &len, "FPS ");  app_uint(lines[count], &len, s.fps);
    app_str(lines[count], &len, "  U ");  app_uint(lines[count], &len, s.underruns);
    app_str(lines[count], &len, "  O ");  app_uint(lines[count], &len, s.overruns);
    count++;

    if (count >= max_lines) return count;          // line 1: AQ queued/target
    len = 0; lines[count][0] = '\0';
    app_str(lines[count], &len, "AQ ");   app_uint(lines[count], &len, s.queued);
    app_str(lines[count], &len, "/");     app_uint(lines[count], &len, s.target);
    count++;

    if (count >= max_lines) return count;          // line 2: ROM base name
    len = 0; lines[count][0] = '\0';
    app_str(lines[count], &len, base_name(s.rom));
    count++;

    if (count >= max_lines) return count;          // line 3: mode  scale
    len = 0; lines[count][0] = '\0';
    if (s.mode)  app_str(lines[count], &len, s.mode);
    app_str(lines[count], &len, "  ");
    if (s.scale) app_str(lines[count], &len, s.scale);
    count++;

    return count;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `make -C test test_hud && ./test/test_hud`
Expected: PASS — `All hud tests passed`.

- [ ] **Step 7: Add `src/ui/hud.o` to the kernel build**

In the root `Makefile`, in the `OBJS` list, after `src/ui/text_canvas.o` add a continuation:

```make
       src/ui/text_canvas.o \
       src/ui/hud.o
```

(Ensure the previous last line `src/ui/text_canvas.o` now ends with ` \`.)

- [ ] **Step 8: Commit**

```bash
git add src/ui/hud.h src/ui/hud.cpp test/test_hud.cpp test/Makefile Makefile
git commit -m "UI: add pure hud_format() diagnostics formatter (host-tested)"
```

---

### Task 2: `debug_overlay` setting (parse/serialize/default)

**Files:**
- Modify: `src/settings/settings.h` (struct field + constructor)
- Modify: `src/settings/settings.cpp` (parse ~line 128, serialize ~line 316)
- Test: `test/test_settings.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `Settings::debug_overlay` (bool, default false); `debug_overlay=on|off` round-trip.

- [ ] **Step 1: Write the failing test**

In `test/test_settings.cpp`, after the existing aspect-mode round-trip block (added previously), add:

```cpp
    // debug_overlay parses, defaults false, round-trips.
    assert(parse_settings("").debug_overlay == false);
    assert(parse_settings("debug_overlay=on\n").debug_overlay == true);
    Settings dbg; dbg.debug_overlay = true;
    char dbgBuf[512];
    serialize_settings(dbg, dbgBuf, sizeof dbgBuf);
    assert(parse_settings(dbgBuf).debug_overlay == true);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -C test test_settings`
Expected: FAIL — `'struct Settings' has no member named 'debug_overlay'`.

- [ ] **Step 3: Add the struct field + default**

In `src/settings/settings.h`, add the field after `vsync` (line 50):

```cpp
    bool       vsync;          // tear-free page flip (default on)
    bool       debug_overlay;  // on-screen diagnostics HUD (default off)
```

And extend the constructor initializer list (after `vsync(true)`):

```cpp
        audio_output(AudioOutput::HDMI), vsync(true), debug_overlay(false)
```

- [ ] **Step 4: Add parse + serialize**

In `src/settings/settings.cpp`, after the `vsync` parse block (lines 128-129):

```cpp
        else if (ieq(key, "vsync"))
            s.vsync = truthy(val);
        else if (ieq(key, "debug_overlay"))
            s.debug_overlay = truthy(val);
```

And after the `vsync` serialize block (lines 315-316), before the trailing `"\n"`:

```cpp
    appendz(out, out_size, "\nvsync=");
    appendz(out, out_size, s.vsync ? "on" : "off");
    appendz(out, out_size, "\ndebug_overlay=");
    appendz(out, out_size, s.debug_overlay ? "on" : "off");
    appendz(out, out_size, "\n");
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make -C test test_settings && ./test/test_settings`
Expected: PASS — `All settings tests passed`.

- [ ] **Step 6: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add debug_overlay key (parse/serialize + host test)"
```

---

### Task 3: `Overlay` device class + kernel member

**Files:**
- Create: `src/ui/overlay.h`, `src/ui/overlay.cpp`
- Modify: `Makefile` (`OBJS`: add `src/ui/overlay.o`)
- Modify: `src/kernel.h` (member `Overlay m_Overlay;`)
- Modify: `src/kernel.cpp` (constructor init list)

**Interfaces:**
- Consumes: `TextCanvas` (`CharW/CharH/FillRect/DrawText`); `HudStats`/`hud_format` from Task 1.
- Produces: `class Overlay { Overlay(TextCanvas*); void SetEnabled(bool); bool Enabled() const; void Draw(const HudStats&); }`. `Draw` is a no-op when disabled.

- [ ] **Step 1: Write `src/ui/overlay.h`**

```cpp
//
// src/ui/overlay.h
//
// Bare Metal Sega Genesis
// Composites the diagnostics HUD onto the game framebuffer via TextCanvas.
// Drawn after retro_run() each frame; no-op when disabled.
//

#ifndef _ui_overlay_h
#define _ui_overlay_h

#include "text_canvas.h"
#include "hud.h"

class Overlay
{
public:
    explicit Overlay(TextCanvas *pCanvas);

    void SetEnabled(bool on) { m_Enabled = on; }
    bool Enabled(void) const { return m_Enabled; }

    void Draw(const HudStats &s);   // no-op if disabled

private:
    TextCanvas *m_pCanvas;
    bool        m_Enabled;
};

#endif
```

- [ ] **Step 2: Write `src/ui/overlay.cpp`**

```cpp
//
// src/ui/overlay.cpp
//
// Bare Metal Sega Genesis
// See overlay.h.
//

#include "overlay.h"

Overlay::Overlay(TextCanvas *pCanvas)
:   m_pCanvas(pCanvas), m_Enabled(false)
{
}

void Overlay::Draw(const HudStats &s)
{
    if (!m_Enabled) return;

    char     lines[HUD_LINES][HUD_COLS + 1];
    unsigned n  = hud_format(s, lines, HUD_LINES);
    int      cw = (int) m_pCanvas->CharW();
    int      ch = (int) m_pCanvas->CharH();

    // Fixed opaque box at top-left so prior text is always overwritten (no
    // ghosting across the two vsync pages).
    int x = cw;
    int y = ch;
    m_pCanvas->FillRect(x, y, cw * (HUD_COLS + 1), ch * (HUD_LINES + 1), 0x0000);
    for (unsigned i = 0; i < n; i++)
    {
        m_pCanvas->DrawText(x + cw / 2, y + ch / 2 + ch * (int) i,
                            lines[i], 0xFFFF, 0x0000);   // white on black
    }
}
```

- [ ] **Step 3: Add `src/ui/overlay.o` to `OBJS`**

In the root `Makefile`, extend the `OBJS` list:

```make
       src/ui/text_canvas.o \
       src/ui/hud.o \
       src/ui/overlay.o
```

- [ ] **Step 4: Add the kernel member**

In `src/kernel.h`, after `TextCanvas m_Canvas;` (line 80) add:

```cpp
	TextCanvas         m_Canvas;     // on-screen UI on the game framebuffer
	Overlay            m_Overlay;    // diagnostics HUD over the game frame
```

Add the include near the other UI include (top of `kernel.h`, alongside the `text_canvas.h` include):

```cpp
#include "ui/overlay.h"
```

- [ ] **Step 5: Construct the member**

In `src/kernel.cpp` constructor init list, after `m_Canvas (&m_Display),` (line 25) add:

```cpp
	m_Canvas (&m_Display),
	m_Overlay (&m_Canvas),
```

- [ ] **Step 6: Build to verify it compiles**

Run: `make`
Expected: builds to completion; `src/ui/overlay.o` and `src/ui/hud.o` compile; kernel image produced.

- [ ] **Step 7: Commit**

```bash
git add src/ui/overlay.h src/ui/overlay.cpp Makefile src/kernel.h src/kernel.cpp
git commit -m "UI: add Overlay class drawing the HUD via TextCanvas"
```

---

### Task 4: Settings screen — Debug Overlay row + live apply

**Files:**
- Modify: `src/menu/settings_screen.h` (constructor param + member)
- Modify: `src/menu/settings_screen.cpp` (NUM_ROWS, Apply, Render, toggle, START indices)
- Modify: `src/kernel.cpp` (pass `&m_Overlay` to `SettingsScreen`)

**Interfaces:**
- Consumes: `Overlay` (Task 3), `Settings::debug_overlay` (Task 2).
- Produces: a `Debug Overlay: < On/Off >` settings row at index 9; Controls/Video Mode shift to indices 10/11; `Apply()` pushes `debug_overlay` to the `Overlay` live.

- [ ] **Step 1: Add the `Overlay*` dependency to `SettingsScreen`**

In `src/menu/settings_screen.h`, add the include:

```cpp
#include "../ui/overlay.h"
```

Extend the constructor signature (add a trailing `Overlay *pOverlay`):

```cpp
    SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay,
                   AudioDriver *pAudio, ControlsScreen *pControls,
                   VideoModeScreen *pVideoMode, Overlay *pOverlay);
```

Add the member after `m_pVideoMode`:

```cpp
    VideoModeScreen *m_pVideoMode;
    Overlay         *m_pOverlay;
```

- [ ] **Step 2: Update the constructor definition**

In `src/menu/settings_screen.cpp`, update the signature and init list:

```cpp
SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay,
                               AudioDriver *pAudio, ControlsScreen *pControls,
                               VideoModeScreen *pVideoMode, Overlay *pOverlay)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay),
    m_pAudio(pAudio), m_pRomPath(0), m_pControls(pControls),
    m_pVideoMode(pVideoMode), m_pOverlay(pOverlay)
{
}
```

- [ ] **Step 3: Apply overlay live**

In `SettingsScreen::Apply()`, add after the audio lines (line 44):

```cpp
    m_pAudio->SetMute(m_pSettings->mute);                // live
    m_pOverlay->SetEnabled(m_pSettings->debug_overlay);  // live
```

- [ ] **Step 4: Bump `NUM_ROWS` and add the row**

In `src/menu/settings_screen.cpp` change (line 16):

```cpp
#define NUM_ROWS 12
```

In `Render`, add the value string after `vsyncVal` (line 91):

```cpp
    const char *vsyncVal = m_pSettings->vsync ? "< On >" : "< Off >";
    const char *dbgVal   = m_pSettings->debug_overlay ? "< On >" : "< Off >";
```

Update the `labels` array to insert `"Debug Overlay:"` at index 9 (before `"Controls..."`):

```cpp
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Audio out:",
                                     "Vsync:", "Debug Overlay:",
                                     "Controls...", "Video Mode..." };
```

Update the `values` array to match (insert `dbgVal` at index 9):

```cpp
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, audioVal,
                                     vsyncVal, dbgVal, "", "" };
```

- [ ] **Step 5: Add the toggle case + shift the START indices**

In the Left/Right `switch`, after `case 8:` (Vsync, lines 211-213) add:

```cpp
            case 8:   // Vsync (toggle tear-free page flip; live)
                m_pSettings->vsync = !m_pSettings->vsync;
                break;
            case 9:   // Debug Overlay (toggle diagnostics HUD; live)
                m_pSettings->debug_overlay = !m_pSettings->debug_overlay;
                break;
```

In the `GP_START` handler, change the two navigation indices (were 9 and 10):

```cpp
            if (selected == 10)                       // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 11)                  // Video Mode...
            {
                m_pVideoMode->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
```

- [ ] **Step 6: Pass `&m_Overlay` from the kernel**

In `src/kernel.cpp`, update the `m_SettingsScreen` init (line 31) to add the trailing argument:

```cpp
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display, &m_Audio, &m_ControlsScreen, &m_VideoModeScreen, &m_Overlay),
```

- [ ] **Step 7: Build + run the full host suite**

Run: `make && make -C test run`
Expected: kernel builds; all host tests pass (`All hud tests passed`, `All settings tests passed`, etc.), no regressions.

- [ ] **Step 8: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp src/kernel.cpp
git commit -m "Settings UI: Debug Overlay row + live apply to Overlay"
```

---

### Task 5: Kernel play-loop integration (measured FPS + draw)

**Files:**
- Modify: `src/kernel.cpp` (boot-apply, FPS measurement, build `HudStats`, draw)

**Interfaces:**
- Consumes: `m_Overlay` (Task 3), `HudStats` (Task 1), `m_Audio.Underruns/Overruns/QueuedFrames`, `video_mode_file_value` (settings), `target`/`romPath` (loop locals).
- Produces: a live HUD when `debug_overlay` is on. No host test (device-only loop wiring).

- [ ] **Step 1: Apply the setting at boot**

In `src/kernel.cpp`, in the boot-apply block (after line 129 `m_Audio.SetMute (...)`), add:

```cpp
	m_Audio.SetMute (m_Settings.mute);
	m_Overlay.SetEnabled (m_Settings.debug_overlay);
```

- [ ] **Step 2: Add a scale-name helper near the top of `kernel.cpp`**

Add a file-static helper (after the includes, before `CKernel::CKernel`):

```cpp
// Short label for the active scale mode, for the HUD.
static const char *scale_name (ScaleMode m)
{
	return m == ScaleMode::Stretch ? "stretch"
	     : m == ScaleMode::Aspect  ? "aspect"
	                               : "integer";
}
```

- [ ] **Step 3: Initialize FPS measurement before the play loop**

In the `--- Play ---` section, alongside `u64 next = CTimer::GetClockTicks64 ();` (line 241), add:

```cpp
		u64      next       = CTimer::GetClockTicks64 ();
		unsigned fpsMeasured = (unsigned) fps;          // seed with nominal
		unsigned fpsFrames   = 0;
		u64      fpsWindow    = next;                   // 1 s window start (us)
```

- [ ] **Step 4: Update FPS once per second + draw the HUD after `retro_run`**

In the play loop, replace the existing `retro_run (); m_Sram.Tick ();` region (lines 276-277) with:

```cpp
			retro_run ();
			m_Sram.Tick ();              // periodic dirty-checked SRAM auto-save

			// Measured FPS: count frames, recompute every ~1 s (ticks are us).
			fpsFrames++;
			u64 fpsNow = CTimer::GetClockTicks64 ();
			if (fpsNow - fpsWindow >= 1000000ULL)
			{
				fpsMeasured = fpsFrames;
				fpsFrames   = 0;
				fpsWindow   = fpsNow;
			}

			if (m_Overlay.Enabled ())
			{
				HudStats st;
				st.fps       = fpsMeasured;
				st.underruns = audioOK ? m_Audio.Underruns ()   : 0;
				st.overruns  = audioOK ? m_Audio.Overruns ()    : 0;
				st.queued    = audioOK ? m_Audio.QueuedFrames () : 0;
				st.target    = target;
				st.rom       = romPath;
				st.mode      = video_mode_file_value (m_Settings.video_mode);
				st.scale     = scale_name (m_Settings.scale_mode);
				m_Overlay.Draw (st);
			}
```

(Note: toggling the HUD off from the pause menu is cleaned up by the existing
`m_Display.ForceRepaint ()` already called after the menu closes — line ~272 —
so no extra clear is needed here.)

- [ ] **Step 5: Build to verify it compiles**

Run: `make`
Expected: builds to completion. If `HudStats`/`Overlay` are unresolved, confirm `#include "ui/overlay.h"` is present in `kernel.h` (Task 3) which transitively includes `hud.h`.

- [ ] **Step 6: Full host suite (regression) + commit**

```bash
make -C test run
git add src/kernel.cpp
git commit -m "Kernel: measured FPS + draw diagnostics HUD after retro_run"
```

Expected: all host tests pass (unchanged by this device-only task).

---

### Task 6: Hardware-verify checklist (section M)

**Files:**
- Modify: `docs/hardware-verification-checklist-2026-06-20.md`

**Interfaces:** none (documentation).

- [ ] **Step 1: Append section M and a summary row**

After the `## L. Aspect-correct scaling` section (before `## Results summary`), add:

```markdown
## M. Diagnostics HUD (debug_overlay)

- [ ] **M1 — Toggle.** Pause → Settings → **Debug Overlay** = On, resume.
  **Expect:** a small text box (top-left) shows FPS, U/O, AQ, ROM, mode/scale.
- [ ] **M2 — Live values.** FPS reads ~60 on a game that keeps up; the U/O and
  AQ numbers update over time and match the ~5 s `audio underruns/overruns` log.
- [ ] **M3 — Context correct.** ROM name and `mode  scale` line match the loaded
  game and current settings (e.g. `1080p  aspect`).
- [ ] **M4 — Clean off.** Set Debug Overlay = Off; **Expect:** the HUD disappears
  with no ghost text left in the letterbox bars or over the game.
- [ ] **M5 — No regression.** With the HUD on, confirm FPS and the underrun count
  do not worsen versus HUD off.
- [ ] **M6 — Persisted.** Confirm `debug_overlay=on` in `SD:/settings.txt`;
  survives reboot (HUD shows from boot).
```

In the Results summary table, add after the `| L. Aspect scaling | | |` row:

```markdown
| M. Diagnostics HUD | | |
```

- [ ] **Step 2: Commit**

```bash
git add docs/hardware-verification-checklist-2026-06-20.md
git commit -m "Docs: add diagnostics HUD hardware-verify checklist (section M)"
```

---

## Self-Review Notes

- **Spec coverage:** compositing approach → Task 5 (draw after `retro_run` on front page); `hud_format` content → Task 1; `Overlay` class → Task 3; settings key → Task 2; settings-screen toggle + live apply → Task 4; measured FPS → Task 5; tests → Tasks 1-2 + regressions in 4-5; checklist M → Task 6. All spec sections mapped.
- **Type consistency:** `HudStats` field names (`fps/underruns/overruns/queued/target/rom/mode/scale`) and `hud_format(const HudStats&, char[][HUD_COLS+1], unsigned)` are identical across Tasks 1, 3, 5. `Overlay(TextCanvas*)`, `SetEnabled/Enabled/Draw` identical across Tasks 3, 4, 5. `Settings::debug_overlay` identical across Tasks 2, 4, 5.
- **Index shift:** Task 4 moves Controls/Video Mode from rows 9/10 to 10/11 in BOTH the arrays and the `GP_START` handler — the one cross-cutting edit to get right.
- **No new blit/Display changes** — overlay draws via existing `TextCanvas` on the front page; vsync/pacing untouched.
