//
// src/menu/rom_menu.cpp
//
// Bare Metal Sega Genesis
// See rom_menu.h. Rendered with GlyphCanvas in the Claude Design v1.0.0 look.
//

#include "rom_menu.h"
#include "menu_path.h"
#include "rom_layout.h"
#include "../ui/theme.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
#include "../input/joypad_map.h"   // GP_UP, GP_DOWN, GP_START
#include "../input/menu_input.h"   // menu_buttons (USB + GPIO)
#include <circle/timer.h>
#include <circle/util.h>           // strcpy, strcmp, strlen

static const char ROOT[] = "SD:/roms";

// Layout constants (pixels), tuned for 1080p; safe on smaller modes via clipping.
static const int PAD      = 40;    // left/right margin
static const int TOP      = 36;    // header baseline area
static const int ROW_H    = 30;    // body row height (VT323 22 + spacing)
static const int LIST_TOP = 96;    // first list row y
static const int FOOT_H   = 40;    // footer band height

// Append unsigned v as decimal into buf at *len (no snprintf on bare metal).
static void app_uint(char *buf, unsigned *len, unsigned v)
{
    char tmp[10];
    int  n = 0;
    if (v == 0) tmp[n++] = '0';
    else while (v) { tmp[n++] = (char) ('0' + v % 10); v /= 10; }
    while (n > 0) buf[(*len)++] = tmp[--n];
    buf[*len] = '\0';
}

static void app_str(char *buf, unsigned *len, const char *s)
{
    while (*s) buf[(*len)++] = *s++;
    buf[*len] = '\0';
}

RomMenu::RomMenu(GlyphCanvas *pCanvas, Gamepad *pGamepad,
                 Storage *pStorage, CUSBHCIDevice *pUSBHCI)
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad),
    m_pStorage(pStorage), m_pUSBHCI(pUSBHCI), m_count(0)
{
    m_path[0] = '\0';
}

void RomMenu::Scan(void)
{
    m_count = 0;
    if (strcmp(m_path, ROOT) != 0)
    {
        m_entries[0].name[0] = '.';
        m_entries[0].name[1] = '.';
        m_entries[0].name[2] = '\0';
        m_entries[0].is_dir = true;
        m_count = 1;
    }
    int got = m_pStorage->ListDir(m_path, &m_entries[m_count],
                                  ROM_MENU_MAX_ENTRIES - m_count);
    if (got > 0) m_count += got;
}

void RomMenu::Render(const MenuState &s)
{
    int W = (int) m_pCanvas->Width();
    int H = (int) m_pCanvas->Height();
    int listBottom = H - FOOT_H - 10;

    m_pCanvas->Clear(theme::BG);

    // Header: title (Press Start 2P x2) + path (cyan) + green P1 dot.
    m_pCanvas->Text(&g_font_ps2p8, 2, PAD, TOP, "ROM BROWSER",
                    theme::WHITE, theme::BG, true);
    int pw = m_pCanvas->TextWidth(&g_font_vt323_22, 1, m_path);
    int pathx = W - PAD - 14 - 24 - pw;        // leave room for dot + "P1"
    m_pCanvas->Text(&g_font_vt323_22, 1, pathx, TOP, m_path,
                    theme::VALUE, theme::BG, true);
    int dotx = W - PAD - 24;
    m_pCanvas->FillRect(dotx, TOP + 4, 10, 10, theme::ACTIVE);
    m_pCanvas->Text(&g_font_vt323_22, 1, dotx + 14, TOP, "P1",
                    theme::ACTIVE, theme::BG, true);
    m_pCanvas->FillRect(PAD, TOP + 34, W - 2 * PAD, 2, theme::TEXT_DIM);

    // List rows.
    int rowW = W - 2 * PAD - 14;       // leave room for the scrollbar
    for (int i = 0; i < s.visible_rows; i++)
    {
        int idx = s.top + i;
        if (idx >= s.count) break;
        int ty = LIST_TOP + i * ROW_H;
        if (ty + ROW_H > listBottom) break;

        bool sel = (idx == s.selected);
        const Entry &e = m_entries[idx];

        if (sel)
            m_pCanvas->FillRect(PAD - 6, ty - 2, rowW + 12, ROW_H, theme::SELECTION);

        int tx = PAD + 22;
        if (sel)
            m_pCanvas->IconPlay(PAD - 2, ty + 4, 16, theme::WHITE);

        if (e.is_dir)
        {
            char buf[270];
            unsigned n = 0;
            app_str(buf, &n, "[ ");
            app_str(buf, &n, e.name);
            app_str(buf, &n, " ]");
            m_pCanvas->Text(&g_font_vt323_22, 1, tx, ty, buf,
                            sel ? theme::WHITE : theme::VALUE, theme::BG, true);
        }
        else
        {
            int ext = rom_ext_offset(e.name);
            if (ext < 0)
            {
                m_pCanvas->Text(&g_font_vt323_22, 1, tx, ty, e.name,
                                sel ? theme::WHITE : theme::TEXT, theme::BG, true);
            }
            else
            {
                char stem[270];
                int k = 0; while (k < ext && k < 268) { stem[k] = e.name[k]; k++; }
                stem[k] = '\0';
                int ex = m_pCanvas->Text(&g_font_vt323_22, 1, tx, ty, stem,
                                         sel ? theme::WHITE : theme::TEXT, theme::BG, true);
                m_pCanvas->Text(&g_font_vt323_22, 1, ex, ty, e.name + ext,
                                sel ? theme::WHITE : theme::TEXT_DIM, theme::BG, true);
            }
        }
    }

    // Scrollbar.
    int trackX = W - PAD - 6;
    int trackY = LIST_TOP;
    int trackH = s.visible_rows * ROW_H;
    m_pCanvas->FillRect(trackX, trackY, 6, trackH, theme::TEXT_DIM);
    ScrollThumb th = scrollbar_thumb(trackH, s.count, s.visible_rows, s.top);
    m_pCanvas->FillRect(trackX, trackY + th.y, 6, th.h, theme::SELECTION);

    // Footer hint bar.
    int fy = H - FOOT_H + 8;
    m_pCanvas->FillRect(PAD, H - FOOT_H - 2, W - 2 * PAD, 2, theme::TEXT_DIM);
    m_pCanvas->IconCross(PAD, fy, 16, theme::TEXT);
    m_pCanvas->Text(&g_font_ps2p8, 1, PAD + 24, fy + 4, "NAVIGATE",
                    theme::TEXT_DIM, theme::BG, true);
    int bx = PAD + 24 + m_pCanvas->TextWidth(&g_font_ps2p8, 1, "NAVIGATE") + 30;
    // "START" launches the highlighted ROM (see Run() below), so render a START
    // pill rather than a single-letter button.
    int sw    = m_pCanvas->TextWidth(&g_font_ps2p8, 1, "START");
    int pillW = sw + 10;
    m_pCanvas->FillRect(bx, fy, pillW, 16, theme::SELECTION);
    m_pCanvas->Text(&g_font_ps2p8, 1, bx + 5, fy + 4, "START",
                    theme::WHITE, theme::SELECTION, true);
    m_pCanvas->Text(&g_font_ps2p8, 1, bx + pillW + 8, fy + 4, "LAUNCH",
                    theme::TEXT_DIM, theme::BG, true);

    // Count, right-aligned: "N ROMS  M FOLDERS".
    int dirs = 0, files = 0;
    for (int i = 0; i < m_count; i++) {
        if (m_entries[i].is_dir) { if (strcmp(m_entries[i].name, "..") != 0) dirs++; }
        else files++;
    }
    char count[48];
    unsigned cl = 0;
    app_uint(count, &cl, (unsigned) files);
    app_str (count, &cl, " ROMS  ");
    app_uint(count, &cl, (unsigned) dirs);
    app_str (count, &cl, " FOLDERS");
    int cw = m_pCanvas->TextWidth(&g_font_ps2p8, 1, count);
    m_pCanvas->Text(&g_font_ps2p8, 1, W - PAD - cw, fy + 4, count,
                    theme::TEXT_DIM, theme::BG, true);

    // CRT scanlines over everything.
    m_pCanvas->Scanlines(0, 0, W, H, 60);
}

bool RomMenu::Run(char *outPath, unsigned outSize)
{
    strcpy(m_path, ROOT);
    Scan();
    if (m_count == 0)
    {
        int W = (int) m_pCanvas->Width();
        int H = (int) m_pCanvas->Height();
        m_pCanvas->Clear(theme::BG);
        m_pCanvas->Text(&g_font_ps2p8, 2, PAD, TOP, "ROM BROWSER",
                        theme::WHITE, theme::BG, true);
        m_pCanvas->FillRect(PAD, TOP + 34, W - 2 * PAD, 2, theme::TEXT_DIM);

        // Cartridge glyph (simple block) centered.
        int cx = W / 2 - 31, cy = H / 2 - 80;
        m_pCanvas->FillRect(cx, cy, 62, 78, theme::TEXT_DIM);
        m_pCanvas->FillRect(cx + 6, cy + 6, 50, 50, theme::BG);

        const char *t1 = "NO ROMS FOUND";
        int t1w = m_pCanvas->TextWidth(&g_font_ps2p8, 2, t1);
        m_pCanvas->Text(&g_font_ps2p8, 2, W / 2 - t1w / 2, H / 2 + 20,
                        t1, theme::WHITE, theme::BG, true);
        const char *t2 = "Place .md / .bin / .gen files in SD:/roms then reboot.";
        int t2w = m_pCanvas->TextWidth(&g_font_vt323_22, 1, t2);
        m_pCanvas->Text(&g_font_vt323_22, 1, W / 2 - t2w / 2, H / 2 + 56,
                        t2, theme::TEXT_MUTED, theme::BG, true);

        m_pCanvas->FillRect(PAD, H - FOOT_H + 6, 10, 10, theme::ADJUST);
        m_pCanvas->Text(&g_font_ps2p8, 1, PAD + 18, H - FOOT_H + 8,
                        "WAITING FOR MEDIA", theme::ADJUST, theme::BG, true);
        m_pCanvas->Scanlines(0, 0, W, H, 60);
        return false;
    }

    int visible = ((int) m_pCanvas->Height() - LIST_TOP - FOOT_H - 12) / ROW_H;
    if (visible < 1) visible = 1;

    MenuState s = { m_count, 0, 0, visible };
    Render(s);

    // Seed prev with the buttons currently held so a still-held confirm (e.g. the
    // Start used to pick "Return to Browser" in the pause menu) isn't seen as a
    // fresh press and doesn't instantly open the first directory. Matches every
    // other menu screen.
    unsigned prev = menu_buttons(m_pGamepad);
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now = menu_buttons(m_pGamepad);
        unsigned pressed = now & ~prev;
        prev = now;

        if (pressed & GP_UP)   { menu_move(&s, -1); Render(s); }
        if (pressed & GP_DOWN) { menu_move(&s, +1); Render(s); }

        if (pressed & GP_START)
        {
            const Entry &e = m_entries[s.selected];
            if (e.is_dir && strcmp(e.name, "..") == 0)
            {
                path_parent(m_path, ROOT, m_path, sizeof m_path);
                Scan(); s.count = m_count; s.selected = 0; s.top = 0; Render(s);
            }
            else if (e.is_dir)
            {
                path_join(m_path, e.name, m_path, sizeof m_path);
                Scan(); s.count = m_count; s.selected = 0; s.top = 0; Render(s);
            }
            else
            {
                path_join(m_path, e.name, outPath, outSize);
                return true;
            }
        }

        CTimer::SimpleMsDelay(16);
    }
}
