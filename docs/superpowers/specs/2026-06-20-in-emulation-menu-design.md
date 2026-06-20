# In-Emulation Menu — Design

**Date:** 2026-06-20
**Status:** Approved (pending implementation plan)
**Milestone:** Phase 2

## Summary

A controller hotkey (Start + Select) opens an overlay menu during gameplay that
pauses emulation. The menu offers Resume, Reset, and Return to ROM Browser
(working), plus greyed Save State / Load State / Settings placeholders. All
on-screen menus move onto the game framebuffer so the browser, gameplay, and the
pause menu share one display with no framebuffer switching.

## Goals

- Open a pause menu with Start + Select during gameplay; emulation pauses and the
  last frame stays visible behind the overlay.
- Working options: **Resume** (continue), **Reset Game** (`retro_reset`),
  **Return to ROM Browser** (unload the game and return to the browser without
  rebooting).
- Show **Save State / Load State / Settings** as disabled ("coming soon")
  entries, skipped during navigation.
- Render all on-screen UI (pause menu and ROM browser) on the game framebuffer
  via a reusable text renderer.

## Non-Goals (deferred)

- Save State / Load State behaviour (depends on the save-state feature).
- Settings screen behaviour (depends on the config feature, FSD §4.9).
- USB keyboard navigation (gamepad only).
- Animations / transparency / partial-alpha overlay (solid box over the frame).
- Per-game audio-rate changes (Genesis rate is constant; audio initialised once).

## Decisions (from brainstorming)

1. **Approach A — unified framebuffer UI.** All menus render on the game
   framebuffer (`m_Display`) via a new `TextCanvas`. `Display.Initialize()` moves
   to boot; the `CScreenDevice` console is used only for early-boot logging. This
   avoids console↔game framebuffer switching (unreliable on bare metal) and is
   the foundation for later Settings / save-state screens.
2. **Options:** all six FSD entries; Save State / Load State / Settings are shown
   but disabled and skipped during navigation.
3. **Hotkey:** Start + Select pressed together (FSD §4.6.4). Edge-detected so it
   fires once and must be released before re-triggering.

## Architecture

### File structure

| Unit | Responsibility | Dependencies | Tested |
|---|---|---|---|
| `src/ui/text_canvas.{h,cpp}` (new) | Draw text + filled rects into `Display`'s RGB565 buffer using `CCharGenerator`. | `CCharGenerator`, `Display` | on-hardware |
| `src/video/display.h` (modify) | Expose `Buffer()/Pitch()/Width()/Height()`. | — | — |
| `src/menu/menu_state.{h,cpp}` (modify) | Add pure `menu_next_enabled`. | none | host |
| `src/menu/pause_menu.{h,cpp}` (new) | `PauseMenu` — entries + `Run() -> MenuAction`. | `TextCanvas`, `Gamepad`, `CUSBHCIDevice` | on-hardware |
| `src/menu/rom_menu.{h,cpp}` (modify) | Render via `TextCanvas` instead of `CScreenDevice`. | `TextCanvas`, … | on-hardware |
| `src/kernel.{h,cpp}` (modify) | Boot-time `Display.Initialize()`; browse↔play loop; hotkey + pause dispatch. | — | — |

### TextCanvas (`src/ui/text_canvas.{h,cpp}`)

The single in-game-UI drawing primitive. Draws into `Display`'s framebuffer, read
through `Display` getters at draw time.

```cpp
class TextCanvas
{
public:
    TextCanvas(Display *pDisplay);   // uses Font12x22 for readability

    unsigned Cols(void) const;       // Width()  / char width
    unsigned Rows(void) const;       // Height() / char height

    void Clear(u16 color);
    void FillRect(int x, int y, int w, int h, u16 color);   // pixel coords, clipped
    // Draw an ASCII string at pixel (x,y); each glyph pixel uses fg, the cell
    // background uses bg. Uses CCharGenerator::GetPixel(c, px, py).
    void DrawText(int x, int y, const char *s, u16 fg, u16 bg);

    unsigned CharW(void) const;      // CCharGenerator::GetCharWidth()
    unsigned CharH(void) const;      // CCharGenerator::GetCharHeight()

private:
    Display        *m_pDisplay;
    CCharGenerator  m_Font;
};
```

Colours are RGB565 `u16` (e.g. white `0xFFFF`, black `0x0000`, grey `0x8410`,
blue `0x001F`). All drawing is clipped to the framebuffer bounds.

### Display getters (`src/video/display.h`)

Add to `Display`:

```cpp
u16     *Buffer(void) const { return m_pBuffer; }
unsigned Pitch (void) const { return m_Pitch; }   // bytes
unsigned Width (void) const { return m_FbW; }
unsigned Height(void) const { return m_FbH; }
```

### Menu navigation helper (`src/menu/menu_state.{h,cpp}`)

```cpp
// Starting from `from`, return the next index in direction `dir` (+1/-1) whose
// enabled[] is true. Stops at the first/last enabled entry (no wrap). If `from`
// itself is the only enabled entry, returns `from`. If none are enabled,
// returns `from`.
int menu_next_enabled(const bool *enabled, int count, int from, int dir);
```

### PauseMenu (`src/menu/pause_menu.{h,cpp}`)

```cpp
enum class MenuAction { Resume, Reset, ReturnToBrowser };

class PauseMenu
{
public:
    PauseMenu(TextCanvas *pCanvas, Gamepad *pGamepad, CUSBHCIDevice *pUSBHCI);
    MenuAction Run(void);     // pauses caller; returns the chosen action
private:
    void Render(int selected);
    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    CUSBHCIDevice *m_pUSBHCI;
};
```

Fixed entries (index → label, enabled, action):

| idx | label | enabled | action |
|---|---|---|---|
| 0 | Resume | yes | Resume |
| 1 | Save State | no | — |
| 2 | Load State | no | — |
| 3 | Reset Game | yes | Reset |
| 4 | Settings | no | — |
| 5 | Return to ROM Browser | yes | ReturnToBrowser |

`Run()`:
1. `selected = 0`; `Render(selected)` (draws a filled box over the last frame +
   the entries; enabled rows in white, disabled in grey, the selection
   highlighted).
2. Loop (~16 ms): `UpdatePlugAndPlay`; `Poll`; edge-detect:
   - Up → `selected = menu_next_enabled(enabled, 6, selected, -1)`; re-render.
   - Down → `selected = menu_next_enabled(enabled, 6, selected, +1)`; re-render.
   - Start (confirm) → return the selected entry's action (only enabled entries
     are reachable).
3. The hotkey bits (Start+Select) must be released before the menu treats Start
   as a confirm, so opening the menu doesn't instantly select Resume.

### RomMenu changes (`src/menu/rom_menu.{h,cpp}`)

- Constructor takes `TextCanvas *` instead of `CScreenDevice *` (plus the
  existing `Gamepad`, `Storage`, `CUSBHCIDevice`).
- `Render` draws via `TextCanvas` (clear, header line, the visible slice with a
  highlighted selection, footer) instead of `CScreenDevice` escape sequences.
- `visible_rows` comes from `m_pCanvas->Rows() - <header/footer lines>`.
- Scroll/selection (`menu_state`), scanning, and path handling are unchanged.

### Kernel changes (`src/kernel.{h,cpp}`)

- New members: `TextCanvas m_Canvas;` (constructed with `&m_Display`),
  `PauseMenu m_PauseMenu;` (constructed with `&m_Canvas`, `&m_Gamepad`,
  `&m_USBHCI`). `RomMenu m_RomMenu` now constructed with `&m_Canvas` (not
  `&m_Screen`).
- `Initialize()`: call `m_Display.Initialize()` at boot (halt on failure).
- `Run()`: set callbacks + `retro_init()` once, then the browse↔play loop
  (see Data Flow). Reset (`retro_reset`) and Return-to-Browser handled via the
  pause-menu action.

## Data flow

```
boot Initialize: screen, serial, logger, interrupt, timer, USBHCI(PnP), EMMC,
                 Storage.Mount(), Display.Initialize(), build TextCanvas
Run: set callbacks; retro_init() once
  for (;;) {                                   # browse <-> play
      RomMenu.Run(romPath)                     # on m_Display
      if !ReadFile(romPath):  show error; continue
      if !retro_load_game():  free buf; show error; continue
      set 6-button port; g_gamepad; g_display (audio initialised once, 1st game)
      action = PlayLoop()
      retro_unload_game(); delete[] ROM buffer
  }

PlayLoop (per frame):
  UpdatePlugAndPlay; Gamepad.Poll
  if Start+Select rising edge:
      switch PauseMenu.Run():
          Resume          -> continue
          Reset           -> retro_reset(); continue
          ReturnToBrowser -> return
  retro_run(); pace (timer + audio watermark); LED blink
```

## Error handling

| Condition | Behavior |
|---|---|
| SD mount fails | Halt (boot-time). |
| No ROMs in `/roms` | `TextCanvas` "No ROMs found"; halt. |
| `ReadFile` fails | `TextCanvas` error; free any buffer; return to the browser. |
| `retro_load_game` fails | `TextCanvas` error; free buffer; return to the browser. |
| Paused (menu open) | `retro_run` not called; audio queue drains to silence; resumes on Resume. |
| ROM buffer | `delete[]` on each `retro_unload_game` — no leak across browse↔play cycles. |

## Testing

**Host (`test/`):**
- `menu_next_enabled`: from an enabled entry, Down/Up land on the next enabled
  entry, skipping disabled ones; clamps at the first/last enabled entry (no
  wrap); returns `from` when no other entry is enabled; safe when all disabled.
- Existing `menu_state`, `rom_filter`, `menu_path` suites still pass.

**Hardware:**
- Start + Select opens the menu over the paused frame; audio pauses.
- Navigation skips the greyed Save State / Load State / Settings; the cursor only
  lands on Resume / Reset Game / Return to ROM Browser.
- Resume continues exactly where paused.
- Reset Game soft-resets the running game.
- Return to ROM Browser shows the browser; selecting another ROM loads and runs
  it.
- A bad/corrupt ROM shows an error and returns to the browser (no hang).
- Repeated browse↔play cycles stay stable (no leak/crash).

## Acceptance criteria

- Start + Select during gameplay opens the overlay menu with emulation paused and
  the last frame visible behind it.
- Resume, Reset Game, and Return to ROM Browser each behave as described; the
  three placeholder entries are visible, greyed, and unreachable by the cursor.
- The ROM browser and the pause menu both render on the game framebuffer (no
  console switching); Return to ROM Browser works without rebooting.
- Loading a different game after returning to the browser works repeatedly.
- Host test for `menu_next_enabled` passes; existing host suites still pass.
