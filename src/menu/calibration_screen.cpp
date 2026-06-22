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
