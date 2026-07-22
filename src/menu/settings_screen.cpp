//
// src/menu/settings_screen.cpp
//
// Bare Metal Sega Genesis
// See settings_screen.h.
//

#include "settings_screen.h"
#include "../input/joypad_map.h"          // GP_UP, GP_DOWN, GP_LEFT, GP_RIGHT, GP_B
#include "../input/menu_input.h"          // menu_buttons (USB + GPIO)
#include "../libretro/environment.h"      // g_widescreen, g_variables_dirty
#include <circle/timer.h>
#include <string.h>   // strcmp, strncpy for the auto-launch row
#include "controls_screen.h"
#include "video_mode_screen.h"
#include "calibration_screen.h"
#include "../ui/theme.h"
#include "../ui/screen_chrome.h"
#include "menu_state.h"

#define NUM_ROWS 16

// Row identities in DISPLAY order (grouped Video / Audio / System / Input).
// The labels/values arrays and the input handlers key off these names, so the
// on-screen order can be changed here (and in the arrays) without touching the
// switch logic. (SRow)i is the identity of the row at display index i.
enum SRow {
    SR_VIDEO_SCALE = 0, SR_WIDESCREEN, SR_VSYNC, SR_VIDEO_MODE,   // Video
    SR_VOLUME, SR_MUTE, SR_AUDIO_OUT, SR_AUDIO_LATENCY,           // Audio
    SR_REGION, SR_MENU_HOTKEY, SR_AUTO_LAUNCH, SR_DEBUG_OVERLAY,  // System
    SR_PAD_TYPE, SR_CONTROLS, SR_HOTKEYS, SR_CALIBRATE            // Input
};

// Rows that open another screen (drawn with a ▸ chevron, opened with START).
static bool is_subscreen(int row)
{
    return row == SR_VIDEO_MODE || row == SR_CONTROLS ||
           row == SR_HOTKEYS    || row == SR_CALIBRATE;
}

SettingsScreen::SettingsScreen(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                               CUSBHCIDevice *pUSBHCI, Settings *pSettings,
                               SettingsStore *pStore, Display *pDisplay,
                               AudioDriver *pAudio, ControlsScreen *pControls,
                               VideoModeScreen *pVideoMode, Overlay *pOverlay,
                               HotkeyScreen *pHotkey, CalibrationScreen *pCalibration)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay),
    m_pAudio(pAudio), m_pRomPath(0), m_pControls(pControls),
    m_pVideoMode(pVideoMode), m_pOverlay(pOverlay), m_pHotkey(pHotkey),
    m_pCalibration(pCalibration)
{
}

void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    m_pDisplay->SetVsync(m_pSettings->vsync);            // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_region_value    = region_core_value(m_pSettings->region);
    g_variables_dirty = true;                            // ...on next poll/reset
    m_pAudio->SetVolume(m_pSettings->volume);            // live
    m_pAudio->SetMute(m_pSettings->mute);                // live
    m_pOverlay->SetEnabled(m_pSettings->debug_overlay);  // live
}

// Format a 0-100 volume as "< NNN >" into out (>= 8 bytes).
static void fmt_volume(char *out, unsigned v)
{
    char rev[4]; int n = 0;
    if (v == 0) rev[n++] = '0';
    else while (v) { rev[n++] = (char) ('0' + v % 10); v /= 10; }
    int i = 0; while (n) out[i++] = rev[--n]; out[i] = '\0';
}

void SettingsScreen::Render(int selected)
{
    using namespace chrome;
    char volVal[8];
    fmt_volume(volVal, m_pSettings->volume);
    const char *scaleVal =
        m_pSettings->scale_mode == ScaleMode::Stretch ? "Stretch" :
        m_pSettings->scale_mode == ScaleMode::Aspect  ? "Aspect"  : "Integer";
    const char *wideVal  = m_pSettings->widescreen ? "On" : "Off";
    const char *muteVal  = m_pSettings->mute ? "On" : "Off";
    const char *regionVal =
        m_pSettings->region == Region::NTSC ? "NTSC" :
        m_pSettings->region == Region::PAL  ? "PAL"  : "Auto";
    bool autoOn = m_pRomPath != 0 &&
                  strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
    const char *autoVal = autoOn ? "On" : "Off";
    const char *hotkeyVal =
        m_pSettings->menu_hotkey == MenuHotkey::StartA ? "Start+A" :
        m_pSettings->menu_hotkey == MenuHotkey::StartB ? "Start+B" :
        m_pSettings->menu_hotkey == MenuHotkey::LR     ? "L+R"     : "Start+Select";
    const char *audioVal =
        m_pSettings->audio_output == AudioOutput::Analog ? "Analog"  :
        m_pSettings->audio_output == AudioOutput::I2S    ? "I2S DAC" : "HDMI";
    const char *vsyncVal = m_pSettings->vsync ? "On" : "Off";
    const char *dbgVal   = m_pSettings->debug_overlay ? "On" : "Off";
    const char *latVal =
        m_pSettings->audio_latency == AudioLatency::Low  ? "Low"  :
        m_pSettings->audio_latency == AudioLatency::High ? "High" : "Medium";
    const char *padVal = m_pSettings->pad_type == PadType::ThreeButton
                             ? "3-button" : "6-button";

    // DISPLAY order — must match enum SRow (Video / Audio / System / Input).
    const char *labels[NUM_ROWS] = {
        "Video Scale", "Widescreen", "Vsync", "Video Mode",            // Video
        "Volume", "Mute", "Audio Out", "Audio Latency",                // Audio
        "Region", "Menu Hotkey", "Auto-Launch ROM", "Debug Overlay",   // System
        "Pad Type", "Controls", "Hotkeys", "Calibrate Controller" };   // Input
    const char *values[NUM_ROWS] = {
        scaleVal, wideVal, vsyncVal, "",
        volVal, muteVal, audioVal, latVal,
        regionVal, hotkeyVal, autoVal, dbgVal,
        padVal, "", "", "" };

    m_pCanvas->Clear(theme::BG);
    header(m_pCanvas, "SETTINGS", "SD:/settings.txt", theme::VALUE);

    // Window the list so it fits the framebuffer height (at 480p all 16 rows +
    // footer would overflow ~480 px). Show the rows that fit and scroll the
    // window so `selected` stays visible; row i draws at on-screen slot i-top.
    int H = (int) m_pCanvas->Height();
    ListWindow win = list_window(H - LIST_TOP - FOOT_H - 12, ROW_H,
                                 NUM_ROWS, selected);
    int visible = win.visible, top = win.top;

    for (int i = top; i < top + visible; i++)
    {
        int vi   = i - top;             // on-screen slot (0..visible-1)
        bool sel = (i == selected);
        if (!is_subscreen(i)) {
            value_row(m_pCanvas, vi, sel, labels[i], values[i], theme::VALUE, true);
        } else {
            int y = value_row(m_pCanvas, vi, sel, labels[i], "", theme::VALUE, false);
            int W = (int) m_pCanvas->Width();
            m_pCanvas->IconTri(W - PAD - 14, y + 5, 12, 0,
                               sel ? theme::WHITE : theme::TEXT_DIM);   // chevron
        }
    }

    footer_divider(m_pCanvas);
    int fy = H - FOOT_H + 4;
    int hx = hint_dpad(m_pCanvas, PAD, fy, "NAVIGATE");
    hx = hint_lr(m_pCanvas, hx, fy, "CHANGE");
    hint_button(m_pCanvas, hx, fy, 'B', theme::TEXT_DIM, "BACK");
    scanlines(m_pCanvas);
}

void SettingsScreen::Run(void)
{
    int selected = 0;
    Render(selected);

    unsigned prev = menu_buttons(m_pGamepad);
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now     = menu_buttons(m_pGamepad);
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
        int dir = 0;
        if (pressed & GP_LEFT)  dir = -1;
        if (pressed & GP_RIGHT) dir = +1;
        if (dir != 0)
        {
            switch ((SRow) selected)
            {
            case SR_VIDEO_SCALE:   // cycle Integer -> Stretch -> Aspect
            {
                // dir is +1 (Right) or -1 (Left); wrap through the 3 modes.
                int n = (int) m_pSettings->scale_mode + (dir > 0 ? 1 : 2);
                m_pSettings->scale_mode = (ScaleMode) (n % 3);
                break;
            }
            case SR_WIDESCREEN:
                m_pSettings->widescreen = !m_pSettings->widescreen;
                break;
            case SR_VSYNC:   // tear-free page flip (live)
                m_pSettings->vsync = !m_pSettings->vsync;
                break;
            case SR_VOLUME:   // +/- 10, clamped 0-100
            {
                int v = (int) m_pSettings->volume + dir * 10;
                if (v < 0)   v = 0;
                if (v > 100) v = 100;
                m_pSettings->volume = (unsigned) v;
                break;
            }
            case SR_MUTE:
                m_pSettings->mute = !m_pSettings->mute;
                break;
            case SR_AUDIO_OUT:   // HDMI -> Analog -> I2S (applies on reboot)
            {
                int a = (int) m_pSettings->audio_output + dir;
                if (a < 0) a = 2;
                if (a > 2) a = 0;
                m_pSettings->audio_output = (AudioOutput) a;
                break;
            }
            case SR_AUDIO_LATENCY:   // Low -> Medium -> High (live)
            {
                int l = (int) m_pSettings->audio_latency + dir;
                if (l < (int) AudioLatency::Low)  l = (int) AudioLatency::Low;
                if (l > (int) AudioLatency::High) l = (int) AudioLatency::High;
                m_pSettings->audio_latency = (AudioLatency) l;
                break;
            }
            case SR_REGION:   // Auto -> NTSC -> PAL
            {
                int r = (int) m_pSettings->region + dir;
                if (r < 0) r = 2;
                if (r > 2) r = 0;
                m_pSettings->region = (Region) r;
                break;
            }
            case SR_MENU_HOTKEY:   // cycle the 4 presets
            {
                int h = (int) m_pSettings->menu_hotkey + dir;
                if (h < 0) h = 3;
                if (h > 3) h = 0;
                m_pSettings->menu_hotkey = (MenuHotkey) h;
                break;
            }
            case SR_AUTO_LAUNCH:   // toggle auto-launch this game
            {
                bool on = m_pRomPath != 0 &&
                          strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
                if (on)
                {
                    m_pSettings->auto_launch_rom[0] = '\0';
                }
                else if (m_pRomPath != 0)
                {
                    strncpy(m_pSettings->auto_launch_rom, m_pRomPath,
                            sizeof(m_pSettings->auto_launch_rom) - 1);
                    m_pSettings->auto_launch_rom[
                        sizeof(m_pSettings->auto_launch_rom) - 1] = '\0';
                }
                break;
            }
            case SR_DEBUG_OVERLAY:   // toggle diagnostics HUD (live)
                m_pSettings->debug_overlay = !m_pSettings->debug_overlay;
                break;
            case SR_PAD_TYPE:   // toggle 3/6-button (applies on ROM reload)
                m_pSettings->pad_type =
                    m_pSettings->pad_type == PadType::SixButton
                        ? PadType::ThreeButton : PadType::SixButton;
                break;
            default:   // sub-screen rows have no left/right action
                break;
            }

            Apply();
            m_pStore->Save(*m_pSettings);
            Render(selected);
        }
        if (pressed & GP_START)
        {
            switch ((SRow) selected)
            {
            case SR_VIDEO_MODE: m_pVideoMode->Run();   break;
            case SR_CONTROLS:   m_pControls->Run();    break;
            case SR_HOTKEYS:    m_pHotkey->Run();       break;
            case SR_CALIBRATE:  m_pCalibration->Run();  break;
            default: break;                            // value rows: START is a no-op
            }
            if (is_subscreen(selected))
            {
                prev = menu_buttons(m_pGamepad);      // ignore buttons held on return
                Render(selected);
            }
        }
        if (pressed & GP_B)
            return;

        CTimer::SimpleMsDelay(16);
    }
}
