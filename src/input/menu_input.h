//
// src/input/menu_input.h
//
// Bare Metal Sega Genesis
// Combined USB + GPIO button read for on-screen menu navigation, so a real
// Sega pad wired to GPIO (sega_board.h) drives every menu the same way it
// already drives in-game input (input_merge.h / callbacks.cpp), instead of
// only the USB gamepad being able to.
//

#ifndef _input_menu_input_h
#define _input_menu_input_h

class Gamepad;

// GP_* bitmask: pGamepad's MenuButtons() OR'd with any GPIO pad on either
// port (via the g_gpio_pads global set once in CKernel::Run).
unsigned menu_buttons(Gamepad *pGamepad);

#endif
