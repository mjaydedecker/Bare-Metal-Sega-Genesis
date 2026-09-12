//
// src/menu/controller_test_screen.cpp
//
// Bare Metal Sega Genesis
// See controller_test_screen.h.
//

#include "controller_test_screen.h"
#include "pad_test.h"
#include "../input/joypad_map.h"   // GP_START
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include "../ui/fonts/font_ps2p8.h"
#include <circle/timer.h>
#include <string.h>

#define NUM_PANELS   4
#define HOLD_EXIT_US 2000000u   // hold Start 2 s to leave
#define PANEL_GAP    12
#define TITLE_H      28         // panel title strip
#define BAR_W        160        // footer hold-progress bar
#define BAR_H        10

// Pad drawing, relative to its origin (see RenderPanel): d-pad cells, face
// buttons (X Y Z over A B C, as on the real pad) and Start/Mode pills.
#define CELL      22
#define BTN_D     28
#define BTN_PITCH 34
#define FACE_X    94
#define PILL_Y    80
#define PILL_H    20
#define DRAW_W    (FACE_X + 2 * BTN_PITCH + BTN_D)   // 190
#define DRAW_H    (PILL_Y + PILL_H)                  // 100

static const char *const TITLES[NUM_PANELS] = { "HAT PORT 1", "HAT PORT 2", "USB 1", "USB 2" };

ControllerTestScreen::ControllerTestScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                                           CUSBHCIDevice *pUSBHCI, GpioPads *pGpioPads)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pGpioPads(pGpioPads), m_BarX(0), m_BarY(0)
{
}

void ControllerTestScreen::Sample(PanelState s[NUM_PANELS])
{
    memset(s, 0, NUM_PANELS * sizeof s[0]);   // zero padding so memcmp is exact
    for (unsigned p = 0; p < 2; p++)
    {
        PanelState &hat = s[p];
        SegaPadType t = m_pGpioPads->PadTypeAt(p);
        hat.present = (t != SegaPadType::None);
        hat.kind    = (uint8_t) t;
        hat.buttons = m_pGpioPads->Buttons(p);

        PanelState &usb = s[2 + p];
        usb.present = m_pGamepad->IsPresent(p);
        if (usb.present)
        {
            usb.buttons = m_pGamepad->Buttons(p);
            usb.vid     = m_pGamepad->VendorId(p);
            usb.pid     = m_pGamepad->ProductId(p);
        }
    }
}

// A face button / d-pad cell / pill colour for a key state.
static void key_colors(PadKeyState st, u16 &fill, u16 &fg)
{
    fill = theme::TEXT_DIM;
    fg   = theme::TEXT;
    switch (st)
    {
    case PadKeyState::Pressed:     fill = theme::ACTIVE;   fg = theme::BG;       break;
    case PadKeyState::Released:    fill = theme::TEXT_DIM; fg = theme::TEXT;     break;
    case PadKeyState::Unavailable: fill = theme::BG;       fg = theme::TEXT_DIM; break;
    }
}

void ControllerTestScreen::RenderPanel(int index, const PanelState &s,
                                       int x, int y, int w, int h)
{
    const Font *f = &g_font_ps2p8;
    bool isHat = index < 2;

    // Frame.
    m_pCanvas->FillRect(x, y, w, 2, theme::TEXT_DIM);
    m_pCanvas->FillRect(x, y + h - 2, w, 2, theme::TEXT_DIM);
    m_pCanvas->FillRect(x, y, 2, h, theme::TEXT_DIM);
    m_pCanvas->FillRect(x + w - 2, y, 2, h, theme::TEXT_DIM);

    // Title + status.
    char vp[10];
    const char *status = "NO PAD";
    u16 statusColor    = theme::TEXT_DIM;
    bool drawPad = false, six = true;
    if (s.present && isHat)
    {
        switch ((SegaPadType) s.kind)
        {
        case SegaPadType::ThreeButton: status = "3-BUTTON"; statusColor = theme::ACTIVE;
                                       drawPad = true; six = false; break;
        case SegaPadType::SixButton:   status = "6-BUTTON"; statusColor = theme::ACTIVE;
                                       drawPad = true; break;
        default:                       status = "UNSUPPORTED"; statusColor = theme::ADJUST; break;
        }
    }
    else if (s.present)
    {
        pad_test_vid_pid(vp, sizeof vp, s.vid, s.pid);
        status = vp; statusColor = theme::VALUE; drawPad = true;
    }
    m_pCanvas->Text(f, 1, x + 12, y + 10, TITLES[index], theme::TEXT, theme::BG, true);
    int sw = m_pCanvas->TextWidth(f, 1, status);
    m_pCanvas->Text(f, 1, x + w - 12 - sw, y + 10, status, statusColor, theme::BG, true);

    if (!drawPad)
    {
        const char *msg = s.present ? "Not a Genesis pad" : "Connect a pad";
        int mw = m_pCanvas->TextWidth(f, 1, msg);
        m_pCanvas->Text(f, 1, x + (w - mw) / 2, y + TITLE_H + (h - TITLE_H) / 2 - 4, msg,
                        s.present ? theme::ADJUST : theme::TEXT_DIM, theme::BG, true);
        return;
    }

    // Pad drawing, centred below the title strip.
    int ox = x + (w - DRAW_W) / 2;
    int oy = y + TITLE_H + (h - TITLE_H - DRAW_H) / 2;
    u16 fill, fg;

    // D-pad: plus shape of cells around a neutral centre.
    static const struct { int key, cx, cy; } DPAD[4] =
    {
        { PTK_UP, 1, 0 }, { PTK_LEFT, 0, 1 }, { PTK_RIGHT, 2, 1 }, { PTK_DOWN, 1, 2 }
    };
    m_pCanvas->FillRect(ox + CELL, oy + CELL, CELL - 2, CELL - 2, theme::TEXT_DIM);
    for (int i = 0; i < 4; i++)
    {
        key_colors(pad_test_key_state(PAD_TEST_KEYS[DPAD[i].key], s.buttons, six), fill, fg);
        m_pCanvas->FillRect(ox + DPAD[i].cx * CELL, oy + DPAD[i].cy * CELL,
                            CELL - 2, CELL - 2, fill);
    }

    // Face buttons: X Y Z (top row), A B C (bottom row).
    static const int FACE[2][3] = { { PTK_X, PTK_Y, PTK_Z }, { PTK_A, PTK_B, PTK_C } };
    for (int row = 0; row < 2; row++)
        for (int col = 0; col < 3; col++)
        {
            const PadTestKey &k = PAD_TEST_KEYS[FACE[row][col]];
            key_colors(pad_test_key_state(k, s.buttons, six), fill, fg);
            m_pCanvas->IconButton(ox + FACE_X + col * BTN_PITCH, oy + row * (BTN_D + 10),
                                  BTN_D, k.label[0], fill, fg, f);
        }

    // Start / Mode pills.
    static const struct { int key, px, pw; const char *text; } PILLS[2] =
    {
        { PTK_START, 0, 72, "START" }, { PTK_MODE, FACE_X, 64, "MODE" }
    };
    for (int i = 0; i < 2; i++)
    {
        key_colors(pad_test_key_state(PAD_TEST_KEYS[PILLS[i].key], s.buttons, six), fill, fg);
        m_pCanvas->FillRect(ox + PILLS[i].px, oy + PILL_Y, PILLS[i].pw, PILL_H, fill);
        int tw = m_pCanvas->TextWidth(f, 1, PILLS[i].text);
        m_pCanvas->Text(f, 1, ox + PILLS[i].px + (PILLS[i].pw - tw) / 2, oy + PILL_Y + 6,
                        PILLS[i].text, fg, fill, true);
    }
}

void ControllerTestScreen::Render(const PanelState s[NUM_PANELS])
{
    using namespace chrome;
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();

    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "CONTROLLER TEST", "HAT + USB", theme::VALUE);

    // 2x2 grid between the header divider and the footer.
    int top = TOP + 48;
    int bottom = H - FOOT_H - 8;
    int pw = (W - 2 * PAD - PANEL_GAP) / 2;
    int ph = (bottom - top - PANEL_GAP) / 2;
    for (int i = 0; i < NUM_PANELS; i++)
        RenderPanel(i, s[i], PAD + (i % 2) * (pw + PANEL_GAP),
                    top + (i / 2) * (ph + PANEL_GAP), pw, ph);

    footer_divider(m_pCanvas);
    int fy = H - FOOT_H + 4;
    m_BarX = hint_start(m_pCanvas, PAD, fy, "HOLD TO EXIT");
    m_BarY = fy + 3;
    scanlines(m_pCanvas);
    RenderProgress(0);
}

void ControllerTestScreen::RenderProgress(unsigned pct)
{
    // Opaque repaint of the bar only (Clear-free), then scanlines over just that
    // rect so it matches the rest of the screen without compounding.
    m_pCanvas->FillRect(m_BarX, m_BarY, BAR_W, BAR_H, theme::TEXT_DIM);
    m_pCanvas->FillRect(m_BarX, m_BarY, (int) (BAR_W * pct / 100), BAR_H, theme::ADJUST);
    m_pCanvas->Scanlines(m_BarX, m_BarY, BAR_W, BAR_H, 60);
}

void ControllerTestScreen::Run(void)
{
    PanelState cur[NUM_PANELS], last[NUM_PANELS];

    m_pUSBHCI->UpdatePlugAndPlay();
    m_pGamepad->Poll();
    m_pGpioPads->Poll();
    Sample(cur);
    Render(cur);
    memcpy(last, cur, sizeof cur);

    bool startHeld = false;
    for (int i = 0; i < NUM_PANELS; i++)
        if (cur[i].buttons & GP_START) startHeld = true;

    HoldExit hold;
    hold_exit_begin(hold, startHeld);   // Start that opened the row doesn't count
    unsigned shownPct = 0;

    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        m_pGpioPads->Poll();
        Sample(cur);

        if (memcmp(cur, last, sizeof cur) != 0)
        {
            Render(cur);                 // redraws the bar empty
            RenderProgress(shownPct);
            memcpy(last, cur, sizeof cur);
        }

        startHeld = false;
        for (int i = 0; i < NUM_PANELS; i++)
            if (cur[i].buttons & GP_START) startHeld = true;

        bool exit = false;
        unsigned pct = hold_exit_update(hold, startHeld, CTimer::GetClockTicks(),
                                        HOLD_EXIT_US, &exit);
        if (exit)
            return;

        pct = pct / 5 * 5;               // repaint the bar in 5% steps
        if (pct != shownPct)
        {
            RenderProgress(pct);
            shownPct = pct;
        }

        CTimer::SimpleMsDelay(16);
    }
}
