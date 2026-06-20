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
#include <circle/util.h>           // strcpy, strcmp, strlen

static const char ROOT[] = "SD:/roms";

RomMenu::RomMenu(CScreenDevice *pScreen, Gamepad *pGamepad,
                 Storage *pStorage, CUSBHCIDevice *pUSBHCI)
:   m_pScreen(pScreen), m_pGamepad(pGamepad),
    m_pStorage(pStorage), m_pUSBHCI(pUSBHCI), m_count(0)
{
    m_path[0] = '\0';
}

static void put(CScreenDevice *s, const char *p)
{
    s->Write(p, strlen(p));
}

void RomMenu::Scan(void)
{
    m_count = 0;
    if (strcmp(m_path, ROOT) != 0)         // not at root: offer a back entry
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
    put(m_pScreen, "\x1b[H\x1b[J");        // cursor home + clear to end = clear
    put(m_pScreen, "  SEGA GENESIS  -  ");
    put(m_pScreen, m_path);
    put(m_pScreen, "\n  --------------------------------\n\n");

    for (int i = 0; i < s.visible_rows; i++)
    {
        int idx = s.top + i;
        if (idx >= s.count) break;
        put(m_pScreen, idx == s.selected ? " > " : "   ");
        const Entry &e = m_entries[idx];
        if (e.is_dir) { put(m_pScreen, "["); put(m_pScreen, e.name); put(m_pScreen, "]"); }
        else          { put(m_pScreen, e.name); }
        put(m_pScreen, "\n");
    }
    put(m_pScreen, "\n  Up/Down: move   Start: open/launch");
}

bool RomMenu::Run(char *outPath, unsigned outSize)
{
    strcpy(m_path, ROOT);
    Scan();
    if (m_count == 0)
    {
        put(m_pScreen, "\x1b[H\x1b[J\n  No ROMs found.\n"
                       "  Place .md/.bin/.gen files in /roms\n");
        return false;
    }

    int visible = (int) m_pScreen->GetRows() - 6;   // header(3) + footer(2) + margin
    if (visible < 1) visible = 1;

    MenuState s = { m_count, 0, 0, visible };
    Render(s);

    unsigned prev = 0;
    for (;;)
    {
        m_pUSBHCI->UpdatePlugAndPlay();     // gamepad enumerates here (PnP)
        m_pGamepad->Poll();
        unsigned now = m_pGamepad->Buttons();
        unsigned pressed = now & ~prev;     // rising edges only
        prev = now;

        if (pressed & GP_UP)   { menu_move(&s, -1); Render(s); }
        if (pressed & GP_DOWN) { menu_move(&s, +1); Render(s); }

        if (pressed & GP_START)
        {
            const Entry &e = m_entries[s.selected];
            if (e.is_dir && strcmp(e.name, "..") == 0)
            {
                path_parent(m_path, ROOT, m_path, sizeof m_path);
                Scan();
                s.count = m_count; s.selected = 0; s.top = 0;
                Render(s);
            }
            else if (e.is_dir)
            {
                path_join(m_path, e.name, m_path, sizeof m_path);
                Scan();
                s.count = m_count; s.selected = 0; s.top = 0;
                Render(s);
            }
            else
            {
                path_join(m_path, e.name, outPath, outSize);
                return true;
            }
        }

        CTimer::SimpleMsDelay(16);          // ~60 Hz input sampling
    }
}
