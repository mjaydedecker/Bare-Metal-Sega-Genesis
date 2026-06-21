#include "../src/input/hotkey.h"
#include "../src/input/joypad_map.h"   // GP_*, hotkey_mask, MenuHotkey
#include <assert.h>
#include <stdio.h>

int main(void)
{
    // Each combo fires when Select held + action freshly pressed.
    assert(decode_hotkey(GP_SELECT | GP_X,    GP_X)    == InGameAction::QuickSave);
    assert(decode_hotkey(GP_SELECT | GP_Y,    GP_Y)    == InGameAction::QuickLoad);
    assert(decode_hotkey(GP_SELECT | GP_A,    GP_A)    == InGameAction::ToggleHud);
    assert(decode_hotkey(GP_SELECT | GP_B,    GP_B)    == InGameAction::Mute);
    assert(decode_hotkey(GP_SELECT | GP_UP,   GP_UP)   == InGameAction::VolUp);
    assert(decode_hotkey(GP_SELECT | GP_DOWN, GP_DOWN) == InGameAction::VolDown);

    // Edge semantics: action held but not freshly pressed -> None.
    assert(decode_hotkey(GP_SELECT | GP_X, 0) == InGameAction::None);
    // Action pressed without Select held -> None.
    assert(decode_hotkey(GP_X, GP_X) == InGameAction::None);

    // No collision with any menu_hotkey preset (held == pressed == preset mask).
    MenuHotkey presets[4] = { MenuHotkey::StartSelect, MenuHotkey::StartA,
                              MenuHotkey::StartB, MenuHotkey::LR };
    for (int i = 0; i < 4; i++)
    {
        unsigned m = hotkey_mask(presets[i]);
        assert(decode_hotkey(m, m) == InGameAction::None);
    }

    printf("All hotkey tests passed\n");
    return 0;
}
