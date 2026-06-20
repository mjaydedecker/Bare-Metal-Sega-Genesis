//
// src/menu/rom_menu.h
//
// Bare Metal Sega Genesis
// On-screen ROM browser: scans SD:/roms, navigates subfolders with the gamepad,
// returns the selected ROM's full path. Renders on the game framebuffer.
//

#ifndef _menu_rom_menu_h
#define _menu_rom_menu_h

#include <circle/usb/usbhcidevice.h>
#include "../ui/text_canvas.h"
#include "../storage/storage.h"
#include "../input/gamepad.h"
#include "menu_state.h"

#define ROM_MENU_MAX_ENTRIES 512

class RomMenu
{
public:
    RomMenu(TextCanvas *pCanvas, Gamepad *pGamepad,
            Storage *pStorage, CUSBHCIDevice *pUSBHCI);

    // Browse from SD:/roms. On launch, writes the selected ROM's full path
    // ("SD:/roms/.../Game.md") to outPath and returns true. Returns false only
    // when the root has no entries at all (caller halts).
    bool Run(char *outPath, unsigned outSize);

private:
    void Scan(void);
    void Render(const MenuState &s);

    TextCanvas    *m_pCanvas;
    Gamepad       *m_pGamepad;
    Storage       *m_pStorage;
    CUSBHCIDevice *m_pUSBHCI;

    char  m_path[300];
    Entry m_entries[ROM_MENU_MAX_ENTRIES];
    int   m_count;
};

#endif
