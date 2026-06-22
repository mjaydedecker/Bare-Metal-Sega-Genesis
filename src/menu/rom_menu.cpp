//
// src/menu/rom_menu.cpp
//
// Bare Metal Sega Genesis
// See rom_menu.h.
//

#include "rom_menu.h"
#include "menu_path.h"
#include "../input/joypad_map.h"   // GP_UP, GP_DOWN, GP_START
#include <circle/timer.h>
#include <circle/util.h>           // strcpy, strcmp

static const char ROOT[] = "SD:/roms";

static const u16 WHITE = 0xFFFF;
static const u16 BLACK = 0x0000;
static const u16 SELFG = 0x0000;
static const u16 SELBG = 0x07FF;

RomMenu::RomMenu(TextCanvas *pCanvas, Gamepad *pGamepad,
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
    int cw = (int) m_pCanvas->CharW();
    int ch = (int) m_pCanvas->CharH();
    int rowW = (int) (m_pCanvas->Cols()) * cw;

    m_pCanvas->Clear(BLACK);
    m_pCanvas->DrawText(cw, 0,     "ROMs:", WHITE, BLACK);
    m_pCanvas->DrawText(cw * 7, 0, m_path,  WHITE, BLACK);

    for (int i = 0; i < s.visible_rows; i++)
    {
        int idx = s.top + i;
        if (idx >= s.count) break;
        int ty = ch * (i + 2);
        bool sel = (idx == s.selected);
        u16 fg = sel ? SELFG : WHITE;
        u16 bg = sel ? SELBG : BLACK;
        if (sel) m_pCanvas->FillRect(0, ty, rowW, ch, SELBG);

        const Entry &e = m_entries[idx];
        m_pCanvas->DrawText(cw, ty, sel ? ">" : " ", fg, bg);
        if (e.is_dir)
        {
            m_pCanvas->DrawText(cw * 3, ty, "[",    fg, bg);
            m_pCanvas->DrawText(cw * 4, ty, e.name, fg, bg);
        }
        else
        {
            m_pCanvas->DrawText(cw * 3, ty, e.name, fg, bg);
        }
    }

    int fy = ch * (s.visible_rows + 3);
    m_pCanvas->DrawText(cw, fy, "Up/Down: move   Start: open/launch", WHITE, BLACK);
}

bool RomMenu::Run(char *outPath, unsigned outSize)
{
    strcpy(m_path, ROOT);
    Scan();
    if (m_count == 0)
    {
        m_pCanvas->Clear(BLACK);
        m_pCanvas->DrawText(40, 40, "No ROMs found.", WHITE, BLACK);
        m_pCanvas->DrawText(40, 40 + (int) m_pCanvas->CharH(),
                            "Place .md/.bin/.gen files in /roms", WHITE, BLACK);
        return false;
    }

    int visible = (int) m_pCanvas->Rows() - 5;   // header(2) + footer(2) + margin
    if (visible < 1) visible = 1;

    MenuState s = { m_count, 0, 0, visible };
    Render(s);

    // Seed prev with the buttons currently held so a still-held confirm (e.g. the
    // Start used to pick "Return to Browser" in the pause menu) isn't seen as a
    // fresh press and doesn't instantly open the first directory. Matches every
    // other menu screen.
    unsigned prev = m_pGamepad->MenuButtons();
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();
        m_pGamepad->Poll();
        unsigned now = m_pGamepad->MenuButtons();
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
