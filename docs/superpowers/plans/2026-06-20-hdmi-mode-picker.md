# Output HDMI Mode Picker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the user pick the HDMI output resolution (Native/1080p/720p/480p) with a confirm-or-revert flow so a mode the TV can't display never sticks.

**Architecture:** A `VideoMode` enum + `video_mode_dims` in settings; `Display::SetMode(w,h)` reallocates the framebuffer (keeping the old one on failure); a `VideoModeScreen` applies a candidate, shows a timed "Keep?" confirm, and reverts (and never saves) on timeout/B — so only a visually-confirmed mode is persisted. The kernel applies the saved mode at boot with a native fallback.

**Tech Stack:** C++ (bare-metal, Circle `CBcmFrameBuffer`), host-side g++ unit tests.

## Global Constraints

- `settings.{h,cpp}` stays free of Circle/libretro (host-testable).
- Mode dims: `Native→(0,0)`, `P1080→(1920,1080)`, `P720→(1280,720)`, `P480→(720,480)`. File keywords: `native`/`1080p`/`720p`/`480p`; unknown → `Native`.
- **Persist a mode only after in-session confirmation** (the safety invariant). Confirm timeout/B reverts and does not save. Boot falls back to native if `SetMode` fails.
- A successful `CBcmFrameBuffer::Initialize()` does NOT prove the TV accepts the signal — the confirm-revert is the real guard.
- Build `-Wall -Wextra`; member-init order matches declaration order.

---

## File Structure

- `src/settings/settings.h` / `.cpp` — `VideoMode`, `video_mode` field, `video_mode_dims`, `video_mode_file_value`, parse/serialize.
- `src/video/display.h` / `.cpp` — `boolean SetMode(unsigned w, unsigned h)`.
- `src/menu/video_mode_screen.h` / `.cpp` — new candidate-pick + confirm-revert screen.
- `src/menu/settings_screen.h` / `.cpp` — `Video Mode...` action row.
- `src/kernel.h` / `.cpp` — own + wire `VideoModeScreen`; apply saved mode at boot.
- `test/test_settings.cpp` — coverage.
- `Makefile` — add `video_mode_screen.o`.

---

## Task 1: VideoMode model + storage

**Files:**
- Modify: `src/settings/settings.h`, `src/settings/settings.cpp`, `test/test_settings.cpp`

**Interfaces:**
- Produces:
  - `enum class VideoMode { Native, P1080, P720, P480 };`
  - `Settings` gains `VideoMode video_mode;` (default `Native`).
  - `void video_mode_dims(VideoMode m, unsigned &w, unsigned &h);`
  - `const char *video_mode_file_value(VideoMode m);`
  - File key `video_mode`.

- [ ] **Step 1: Write the failing test additions**

In `test/test_settings.cpp`, add before `printf("All settings tests passed\n");`:

```cpp
    // Video mode dims.
    {
        unsigned w = 9, h = 9;
        video_mode_dims(VideoMode::Native, w, h); assert(w == 0 && h == 0);
        video_mode_dims(VideoMode::P1080, w, h);  assert(w == 1920 && h == 1080);
        video_mode_dims(VideoMode::P720, w, h);   assert(w == 1280 && h == 720);
        video_mode_dims(VideoMode::P480, w, h);   assert(w == 720 && h == 480);
    }
    // Video mode defaults + parse + file value + round-trip.
    assert(d.video_mode == VideoMode::Native);
    assert(parse_settings("video_mode=720p\n").video_mode == VideoMode::P720);
    assert(parse_settings("video_mode=1080p\n").video_mode == VideoMode::P1080);
    assert(parse_settings("video_mode=480p\n").video_mode == VideoMode::P480);
    assert(parse_settings("video_mode=bogus\n").video_mode == VideoMode::Native);
    assert(strcmp(video_mode_file_value(VideoMode::P720), "720p") == 0);
    assert(strcmp(video_mode_file_value(VideoMode::Native), "native") == 0);
    Settings vms; vms.video_mode = VideoMode::P480;
    char vmbuf[512];
    serialize_settings(vms, vmbuf, sizeof vmbuf);
    assert(parse_settings(vmbuf).video_mode == VideoMode::P480);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_settings`
Expected: FAIL — `'VideoMode' has not been declared`.

- [ ] **Step 3: Add the enum + field + declarations to `settings.h`**

In `src/settings/settings.h`, after the `enum class PadButton ...` line (or near
the other enums), add:

```cpp
enum class VideoMode { Native, P1080, P720, P480 };
```

In `Settings`, add the field after `map2`:

```cpp
    ButtonMap  map2;        // Player 2 button map
    VideoMode  video_mode;  // HDMI output mode
```

Initialize it in the constructor init list (append after the last member, e.g.
after `menu_hotkey(...)`):

```cpp
        menu_hotkey(MenuHotkey::StartSelect), video_mode(VideoMode::Native)
```

After the `parse_button_map` declaration, add:

```cpp
// HDMI output dimensions for a VideoMode (Native => 0,0 = firmware's mode).
void video_mode_dims(VideoMode m, unsigned &w, unsigned &h);

// VideoMode as written to the settings file ("native"|"1080p"|"720p"|"480p").
const char *video_mode_file_value(VideoMode m);
```

- [ ] **Step 4: Implement the helpers + parse + serialize in `settings.cpp`**

In `src/settings/settings.cpp`, add the helpers above `serialize_settings`:

```cpp
void video_mode_dims(VideoMode m, unsigned &w, unsigned &h)
{
    switch (m)
    {
    case VideoMode::P1080: w = 1920; h = 1080; break;
    case VideoMode::P720:  w = 1280; h = 720;  break;
    case VideoMode::P480:  w = 720;  h = 480;  break;
    default:               w = 0;    h = 0;    break;   // Native
    }
}

const char *video_mode_file_value(VideoMode m)
{
    switch (m)
    {
    case VideoMode::P1080: return "1080p";
    case VideoMode::P720:  return "720p";
    case VideoMode::P480:  return "480p";
    default:               return "native";
    }
}
```

In `parse_settings`, add before `// unknown keys: ignored`:

```cpp
        else if (ieq(key, "video_mode"))
        {
            if      (ieq(val, "1080p")) s.video_mode = VideoMode::P1080;
            else if (ieq(val, "720p"))  s.video_mode = VideoMode::P720;
            else if (ieq(val, "480p"))  s.video_mode = VideoMode::P480;
            else                        s.video_mode = VideoMode::Native;
        }
```

In `serialize_settings`, replace the `controller_2_map` tail:

```cpp
    appendz(out, out_size, "\ncontroller_2_map=");
    append_map(out, out_size, s.map2);
    appendz(out, out_size, "\n");
```

with:

```cpp
    appendz(out, out_size, "\ncontroller_2_map=");
    append_map(out, out_size, s.map2);
    appendz(out, out_size, "\nvideo_mode=");
    appendz(out, out_size, video_mode_file_value(s.video_mode));
    appendz(out, out_size, "\n");
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: `All settings tests passed`

- [ ] **Step 6: Build the kernel**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 7: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp test/test_settings.cpp
git commit -m "Settings: add video_mode field + dims/keyword helpers"
```

---

## Task 2: Display::SetMode

**Files:**
- Modify: `src/video/display.h`, `src/video/display.cpp`

**Interfaces:**
- Produces: `boolean Display::SetMode(unsigned w, unsigned h);` — reallocate the
  framebuffer to `(w,h)` (0,0 = native); on success swap and return TRUE; on
  failure keep the existing framebuffer and return FALSE.

- [ ] **Step 1: Declare SetMode**

In `src/video/display.h`, after the `boolean Initialize(void);` declaration, add:

```cpp
    // Re-create the framebuffer at a new HDMI mode (w=h=0 => firmware native).
    // On success swaps to the new mode and returns TRUE; on failure keeps the
    // current mode and returns FALSE. NOTE: success does not prove the TV
    // accepts the signal — callers must confirm-or-revert.
    boolean SetMode(unsigned w, unsigned h);
```

- [ ] **Step 2: Implement SetMode**

In `src/video/display.cpp`, add after `Display::Initialize`:

```cpp
boolean Display::SetMode(unsigned w, unsigned h)
{
    // Build the new framebuffer BEFORE discarding the current one, so a failure
    // leaves the existing mode intact.
    CBcmFrameBuffer *pNew = new CBcmFrameBuffer(w, h, FB_DEPTH);
    if (pNew == 0)
    {
        return FALSE;
    }
    if (!pNew->Initialize() || pNew->GetBuffer() == 0)
    {
        delete pNew;
        return FALSE;
    }

    delete m_pFB;
    m_pFB     = pNew;
    m_pBuffer = (u16 *) (uintptr_t) pNew->GetBuffer();
    m_Pitch   = pNew->GetPitch();
    m_FbW     = pNew->GetWidth();
    m_FbH     = pNew->GetHeight();
    m_LastW   = 0;
    m_LastH   = 0;
    ClearBlack();
    return TRUE;
}
```

- [ ] **Step 3: Build to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 4: Commit**

```bash
git add src/video/display.h src/video/display.cpp
git commit -m "Video: Display::SetMode reallocates the framebuffer (keep-on-fail)"
```

---

## Task 3: VideoModeScreen (pick + confirm-revert)

**Files:**
- Create: `src/menu/video_mode_screen.h`, `src/menu/video_mode_screen.cpp`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `VideoMode`/`video_mode_dims` (Task 1); `Display::SetMode` (Task 2);
  `Gamepad::MenuButtons/Poll`; `SettingsStore::Save`; `GP_*` (`joypad_map.h`).
- Produces: `class VideoModeScreen { VideoModeScreen(TextCanvas*, Gamepad*, CUSBHCIDevice*, Settings*, SettingsStore*, Display*); void Run(); };`

- [ ] **Step 1: Write the header**

Create `src/menu/video_mode_screen.h`:

```cpp
//
// src/menu/video_mode_screen.h
//
// Bare Metal Sega Genesis
// HDMI output-mode picker with a confirm-or-revert guard: applying a mode the
// TV can't display auto-reverts, and only a confirmed mode is persisted.
//

#ifndef _menu_video_mode_screen_h
#define _menu_video_mode_screen_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../input/gamepad.h"
#include "../settings/settings.h"
#include "../settings/settings_store.h"
#include "../video/display.h"

class VideoModeScreen
{
public:
    VideoModeScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                    Settings *pSettings, SettingsStore *pStore, Display *pDisplay);
    void Run(void);   // returns when the user backs out (B)

private:
    void    Render(VideoMode sel);
    void    Apply(VideoMode want);   // SetMode + confirm-or-revert + maybe save
    boolean Confirm(void);           // ~15s countdown; A=keep, B/timeout=revert

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

Create `src/menu/video_mode_screen.cpp`:

```cpp
//
// src/menu/video_mode_screen.cpp
//
// Bare Metal Sega Genesis
// See video_mode_screen.h.
//

#include "video_mode_screen.h"
#include "../input/joypad_map.h"   // GP_LEFT/RIGHT/A/B/START
#include <circle/timer.h>

#define NUM_MODES 4

static const u16 BOX   = 0x0008;
static const u16 WHITE = 0xFFFF;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

static const char *mode_label(VideoMode m)
{
    switch (m)
    {
    case VideoMode::P1080: return "1080p";
    case VideoMode::P720:  return "720p";
    case VideoMode::P480:  return "480p";
    default:               return "Native";
    }
}

VideoModeScreen::VideoModeScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                                 CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                                 SettingsStore *pStore, Display *pDisplay)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay)
{
}

void VideoModeScreen::Render(VideoMode sel)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int boxX = cw * 3, boxY = ch * 2, boxW = cw * 34, boxH = ch * 6;

    m_pCanvas->FillRect(boxX, boxY, boxW, boxH, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch, "VIDEO MODE", WHITE, BOX);

    char val[16];
    int i = 0; val[i++] = '<'; val[i++] = ' ';
    const char *lbl = mode_label(sel);
    for (int j = 0; lbl[j] && i < 13; j++) val[i++] = lbl[j];
    val[i++] = ' '; val[i++] = '>'; val[i] = '\0';
    m_pCanvas->FillRect(boxX + cw, boxY + ch * 3, boxW - cw * 2, ch, SELBG);
    m_pCanvas->DrawText(boxX + cw * 2, boxY + ch * 3, val, SELFG, SELBG);

    m_pCanvas->DrawText(boxX + cw, boxY + ch * 5,
                        "Start: apply   B: back", WHITE, BOX);
}

boolean VideoModeScreen::Confirm(void)
{
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    unsigned prev = m_pGamepad->MenuButtons();

    for (int sec = 15; sec > 0; sec--)
    {
        m_pCanvas->FillRect(cw * 3, ch * 2, cw * 34, ch * 4, BOX);
        m_pCanvas->DrawText(cw * 4, ch * 3, "Keep this mode?", WHITE, BOX);
        char line[40] = "A: keep    Reverting in 00";
        line[24] = (char) ('0' + (sec / 10));
        line[25] = (char) ('0' + (sec % 10));
        m_pCanvas->DrawText(cw * 4, ch * 4, line, WHITE, BOX);

        for (int t = 0; t < 62; t++)   // ~1 s of 16 ms polls
        {
            m_pUSBHCI->UpdatePlugAndPlay();
            m_pGamepad->Poll();
            unsigned now     = m_pGamepad->MenuButtons();
            unsigned pressed = now & ~prev;
            prev = now;
            if (pressed & GP_A) return TRUE;
            if (pressed & GP_B) return FALSE;
            CTimer::SimpleMsDelay(16);
        }
    }
    return FALSE;
}

void VideoModeScreen::Apply(VideoMode want)
{
    VideoMode prevMode = m_pSettings->video_mode;
    unsigned pw, ph; video_mode_dims(prevMode, pw, ph);
    unsigned w, h;   video_mode_dims(want, w, h);

    if (!m_pDisplay->SetMode(w, h))
    {
        // Couldn't even allocate the mode; stay on the current one.
        m_pCanvas->FillRect((int) m_pCanvas->CharW() * 3,
                            (int) m_pCanvas->CharH() * 2,
                            (int) m_pCanvas->CharW() * 34,
                            (int) m_pCanvas->CharH() * 3, BOX);
        m_pCanvas->DrawText((int) m_pCanvas->CharW() * 4,
                            (int) m_pCanvas->CharH() * 3,
                            "Mode unavailable.", WHITE, BOX);
        CTimer::SimpleMsDelay(1200);
        return;
    }

    if (Confirm())
    {
        m_pSettings->video_mode = want;
        m_pStore->Save(*m_pSettings);
    }
    else
    {
        m_pDisplay->SetMode(pw, ph);   // revert to the known-good mode
    }
}

void VideoModeScreen::Run(void)
{
    VideoMode sel = m_pSettings->video_mode;
    Render(sel);

    unsigned prev = m_pGamepad->MenuButtons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = m_pGamepad->MenuButtons();
        unsigned pressed = now & ~prev;
        prev = now;

        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            int v = ((int) sel + dir + NUM_MODES) % NUM_MODES;
            sel = (VideoMode) v;
            Render(sel);
        }

        if (pressed & GP_START)
        {
            Apply(sel);
            sel = m_pSettings->video_mode;            // reflect what stuck
            prev = m_pGamepad->MenuButtons();         // resync
            Render(sel);
        }

        if (pressed & GP_B)
        {
            return;
        }

        CTimer::SimpleMsDelay(16);
    }
}
```

- [ ] **Step 3: Add the object to the `Makefile`**

In `Makefile`, add to `OBJS` after `src/menu/controls_screen.o`:

```makefile
       src/menu/video_mode_screen.o \
```

- [ ] **Step 4: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors. (Linked but not yet opened.)

- [ ] **Step 5: Commit**

```bash
git add src/menu/video_mode_screen.h src/menu/video_mode_screen.cpp Makefile
git commit -m "Video: VideoModeScreen picker with confirm-or-revert"
```

---

## Task 4: Settings-screen row + kernel wiring + boot apply

**Files:**
- Modify: `src/menu/settings_screen.h`, `src/menu/settings_screen.cpp`
- Modify: `src/kernel.h`, `src/kernel.cpp`

**Interfaces:**
- Consumes: `VideoModeScreen` (Task 3); `video_mode_dims`/`Settings.video_mode`
  (Task 1); `Display::SetMode` (Task 2).
- Produces: `SettingsScreen` constructor gains a `VideoModeScreen *pVideoMode`
  parameter (appended last).

- [ ] **Step 1: Add VideoModeScreen to the Settings-screen header**

In `src/menu/settings_screen.h`, after `class ControlsScreen;`, add:

```cpp
class VideoModeScreen;
```

Change the constructor declaration to append `VideoModeScreen *pVideoMode`:

```cpp
    SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI,
                   Settings *pSettings, SettingsStore *pStore, Display *pDisplay,
                   AudioDriver *pAudio, ControlsScreen *pControls,
                   VideoModeScreen *pVideoMode);
```

In the `private:` member list, after `ControlsScreen *m_pControls;`, add:

```cpp
    VideoModeScreen *m_pVideoMode;
```

- [ ] **Step 2: Wire the constructor + include**

In `src/menu/settings_screen.cpp`, add after `#include "controls_screen.h"`:

```cpp
#include "video_mode_screen.h"
```

Update the constructor signature + init list:

```cpp
SettingsScreen::SettingsScreen(TextCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay,
                               AudioDriver *pAudio, ControlsScreen *pControls,
                               VideoModeScreen *pVideoMode)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay),
    m_pAudio(pAudio), m_pRomPath(0), m_pControls(pControls),
    m_pVideoMode(pVideoMode)
{
}
```

- [ ] **Step 3: Grow rows + add the label**

In `src/menu/settings_screen.cpp`, change `#define NUM_ROWS 8` to:

```cpp
#define NUM_ROWS 9
```

Append `Video Mode...` to the label/value arrays:

```cpp
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:",
                                     "Menu Hotkey:", "Controls...",
                                     "Video Mode..." };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal, hotkeyVal, "", "" };
```

- [ ] **Step 4: Open the picker from the action row**

In `src/menu/settings_screen.cpp`, replace the existing Controls Start handler:

```cpp
        if ((pressed & GP_START) && selected == 7)   // Controls... (action row)
        {
            m_pControls->Run();
            prev = m_pGamepad->MenuButtons();         // resync after the sub-screen
            Render(selected);
        }
```

with:

```cpp
        if (pressed & GP_START)
        {
            if (selected == 7)                        // Controls...
            {
                m_pControls->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
            else if (selected == 8)                   // Video Mode...
            {
                m_pVideoMode->Run();
                prev = m_pGamepad->MenuButtons();
                Render(selected);
            }
        }
```

- [ ] **Step 5: Declare + construct VideoModeScreen in the kernel**

In `src/kernel.h`, after `#include "menu/controls_screen.h"`, add:

```cpp
#include "menu/video_mode_screen.h"
```

In the member list, immediately before `ControlsScreen m_ControlsScreen;`, add:

```cpp
	VideoModeScreen    m_VideoModeScreen; // HDMI mode picker
```

- [ ] **Step 6: Wire the kernel constructor**

In `src/kernel.cpp`, in the constructor init list, replace the
`m_ControlsScreen (...)` + `m_SettingsScreen (...)` lines with (order matches
declaration order — `m_VideoModeScreen` first):

```cpp
	m_VideoModeScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display),
	m_ControlsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore),
	m_SettingsScreen (&m_Canvas, &m_Gamepad, &m_USBHCI, &m_Settings, &m_SettingsStore, &m_Display, &m_Audio, &m_ControlsScreen, &m_VideoModeScreen),
```

- [ ] **Step 7: Apply the saved mode at boot**

In `src/kernel.cpp`, after the `g_map0`/`g_map1` lines, add:

```cpp
	if (m_Settings.video_mode != VideoMode::Native)
	{
		unsigned vmW, vmH;
		video_mode_dims (m_Settings.video_mode, vmW, vmH);
		if (!m_Display.SetMode (vmW, vmH))
		{
			m_Logger.Write (FromKernel, LogWarning,
				"Saved video mode unavailable; using native");
		}
	}
```

- [ ] **Step 8: Build and run the full host suite**

Run: `make`
Expected: builds to `kernel7.img`, no errors or `-Wreorder` warnings.

Run: `cd test && make run`
Expected: all suites pass.

- [ ] **Step 9: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp src/kernel.h src/kernel.cpp
git commit -m "Video: open the HDMI mode picker from Settings + apply at boot"
```

---

## Hardware Verification (manual, after Task 4)

Not a code task — perform on the Pi:

- [ ] In a game, Settings → Video Mode... → select 720p → Start → "Keep this mode?" appears → A keeps it; reboot → still 720p, game centers correctly.
- [ ] Select a mode the TV rejects → black → wait out the countdown → the screen returns to the previous mode; `SD:/settings.txt` unchanged.
- [ ] Confirm both integer and stretch scaling still center correctly at the new resolution.

---

## Self-Review Notes

- **Spec coverage:** `VideoMode`/dims/keyword + parse/serialize (Task 1); `Display::SetMode` keep-on-fail (Task 2); `VideoModeScreen` select + apply + ~15s confirm + revert + save-only-on-confirm (Task 3); Settings `Video Mode...` row + kernel wiring + boot apply with native fallback (Task 4); host tests for dims/parse/round-trip (Task 1); hardware checklist incl. the reject→revert path. EDID enumeration / custom modes / refresh-rate are out of scope per the spec — no task.
- **Safety invariant:** persistence happens only inside `Confirm()==TRUE` (Task 3); timeout/B reverts via `SetMode(prev)` without saving; boot falls back to native on `SetMode` failure (Task 4). So a saved mode always displayed at least once.
- **Type consistency:** `VideoMode`/`video_mode_dims`/`video_mode_file_value` (Task 1) used in Tasks 3–4; `Display::SetMode` (Task 2) used in Tasks 3–4; `VideoModeScreen` ctor (Task 3) matches kernel construction (Task 4); `SettingsScreen` 9-arg ctor consistent across header/impl/kernel.
- **Green builds:** Task 3's screen links unused until Task 4 opens it; each task ends with a clean full `make`.
