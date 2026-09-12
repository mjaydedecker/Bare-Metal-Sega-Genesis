//
// src/input/menu_input.cpp
//
#include "menu_input.h"
#include "gamepad.h"
#include "gpio_pads.h"
#include "input_merge.h"
#include "../libretro/callbacks.h"   // extern g_gpio_pads

unsigned menu_buttons(Gamepad *pGamepad)
{
    unsigned btn = pGamepad->MenuButtons();
    if (g_gpio_pads != 0)
    {
        // Menus don't run the game loop's per-frame Poll, so read the DB9 pads
        // here (GpioPads::Poll skips back-to-back calls itself).
        g_gpio_pads->Poll();
        btn = merge_buttons(merge_buttons(btn, g_gpio_pads->Buttons(0)),
                            g_gpio_pads->Buttons(1));
    }
    return btn;
}
