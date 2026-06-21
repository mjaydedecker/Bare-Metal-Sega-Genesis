#include "../src/input/hotkey.h"
#include "../src/input/joypad_map.h"   // GP_*, hotkey_mask, MenuHotkey
#include "../src/settings/settings.h"  // HotkeyBindings, HK_*, PadButton
#include <assert.h>
#include <stdio.h>

int main(void)
{
    HotkeyBindings hk;   // default bindings: Select + X/Y/A/B and L/R

    // Each default combo fires for its action.
    assert(decode_hotkey(GP_SELECT | GP_X, GP_X, hk) == InGameAction::QuickSave);
    assert(decode_hotkey(GP_SELECT | GP_Y, GP_Y, hk) == InGameAction::QuickLoad);
    assert(decode_hotkey(GP_SELECT | GP_LB, GP_LB, hk) == InGameAction::VolUp);
    assert(decode_hotkey(GP_SELECT | GP_RB, GP_RB, hk) == InGameAction::VolDown);
    assert(decode_hotkey(GP_SELECT | GP_A, GP_A, hk) == InGameAction::ToggleHud);
    assert(decode_hotkey(GP_SELECT | GP_B, GP_B, hk) == InGameAction::Mute);

    // Hold not held / trigger not pressed -> None.
    assert(decode_hotkey(GP_X, GP_X, hk) == InGameAction::None);
    assert(decode_hotkey(GP_SELECT | GP_X, 0, hk) == InGameAction::None);

    // Priority: two actions sharing a combo -> lower index wins.
    HotkeyBindings dup = hk;
    dup.b[HK_MUTE] = { PadButton::Select, PadButton::X };   // same as QuickSave
    assert(decode_hotkey(GP_SELECT | GP_X, GP_X, dup) == InGameAction::QuickSave);

    // hotkey_hold_mask of defaults == GP_SELECT.
    assert(hotkey_hold_mask(hk) == GP_SELECT);

    // No conflicts on defaults, vs every menu preset.
    MenuHotkey presets[4] = { MenuHotkey::StartSelect, MenuHotkey::StartA,
                              MenuHotkey::StartB, MenuHotkey::LR };
    for (int i = 0; i < 4; i++)
        assert(hotkey_conflicts(hk, presets[i]) == 0);

    // Duplicate combo flags both actions.
    unsigned c = hotkey_conflicts(dup, MenuHotkey::StartSelect);
    assert((c & (1u << HK_QUICKSAVE)) && (c & (1u << HK_MUTE)));

    // A combo equal to a menu preset flags that action.
    HotkeyBindings clash = hk;
    clash.b[HK_MUTE] = { PadButton::Start, PadButton::Select };  // == Start+Select
    assert(hotkey_conflicts(clash, MenuHotkey::StartSelect) & (1u << HK_MUTE));

    printf("All hotkey tests passed\n");
    return 0;
}
