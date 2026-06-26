# Host-side unit tests (system compiler, not the cross toolchain).
CXX      ?= c++
CXXFLAGS ?= -Wall -Wextra -O2

LIBRETRO_INC = ../libs/genesis-plus-gx-wide/libretro/libretro-common/include

.PHONY: run clean

run: test_blit test_pixel_ops test_glyph_draw test_icons test_theme test_fonts test_rom_layout test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings test_audio_util test_pad_reconcile test_hud test_hotkey test_splash test_controller_map test_sega_board test_sega_pad
	./test_blit
	./test_pixel_ops
	./test_glyph_draw
	./test_icons
	./test_theme
	./test_fonts
	./test_rom_layout
	./test_joypad
	./test_rom_filter
	./test_menu_state
	./test_menu_path
	./test_save_path
	./test_settings
	./test_audio_util
	./test_pad_reconcile
	./test_hud
	./test_hotkey
	./test_splash
	./test_controller_map
	./test_sega_board
	./test_sega_pad

test_blit: test_blit.cpp ../src/video/blit.cpp ../src/video/blit.h
	$(CXX) $(CXXFLAGS) -o $@ test_blit.cpp ../src/video/blit.cpp

test_pixel_ops: test_pixel_ops.cpp ../src/ui/pixel_ops.cpp ../src/ui/pixel_ops.h
	$(CXX) $(CXXFLAGS) -o $@ test_pixel_ops.cpp ../src/ui/pixel_ops.cpp

test_glyph_draw: test_glyph_draw.cpp ../src/ui/glyph_draw.cpp ../src/ui/glyph_draw.h ../src/ui/font.h ../src/ui/pixel_ops.cpp ../src/ui/pixel_ops.h
	$(CXX) $(CXXFLAGS) -o $@ test_glyph_draw.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp

test_icons: test_icons.cpp ../src/ui/icons.cpp ../src/ui/icons.h ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_icons.cpp ../src/ui/icons.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp

test_theme: test_theme.cpp ../src/ui/theme.h
	$(CXX) $(CXXFLAGS) -o $@ test_theme.cpp

test_fonts: test_fonts.cpp ../src/ui/fonts/font_ps2p8.cpp ../src/ui/fonts/font_vt323_16.cpp ../src/ui/fonts/font_vt323_22.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_fonts.cpp ../src/ui/fonts/font_ps2p8.cpp ../src/ui/fonts/font_vt323_16.cpp ../src/ui/fonts/font_vt323_22.cpp ../src/ui/glyph_draw.cpp ../src/ui/pixel_ops.cpp

test_rom_layout: test_rom_layout.cpp ../src/menu/rom_layout.cpp ../src/menu/rom_layout.h
	$(CXX) $(CXXFLAGS) -o $@ test_rom_layout.cpp ../src/menu/rom_layout.cpp

test_joypad: test_joypad.cpp ../src/input/joypad_map.cpp ../src/input/joypad_map.h
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_joypad.cpp ../src/input/joypad_map.cpp

test_rom_filter: test_rom_filter.cpp ../src/menu/rom_filter.cpp ../src/menu/rom_filter.h
	$(CXX) $(CXXFLAGS) -o $@ test_rom_filter.cpp ../src/menu/rom_filter.cpp

test_menu_state: test_menu_state.cpp ../src/menu/menu_state.cpp ../src/menu/menu_state.h
	$(CXX) $(CXXFLAGS) -o $@ test_menu_state.cpp ../src/menu/menu_state.cpp

test_menu_path: test_menu_path.cpp ../src/menu/menu_path.cpp ../src/menu/menu_path.h
	$(CXX) $(CXXFLAGS) -o $@ test_menu_path.cpp ../src/menu/menu_path.cpp

test_save_path: test_save_path.cpp ../src/menu/save_path.cpp ../src/menu/save_path.h
	$(CXX) $(CXXFLAGS) -o $@ test_save_path.cpp ../src/menu/save_path.cpp

test_settings: test_settings.cpp ../src/settings/settings.cpp ../src/settings/settings.h
	$(CXX) $(CXXFLAGS) -o $@ test_settings.cpp ../src/settings/settings.cpp

test_audio_util: test_audio_util.cpp ../src/audio/audio_util.cpp ../src/audio/audio_util.h
	$(CXX) $(CXXFLAGS) -o $@ test_audio_util.cpp ../src/audio/audio_util.cpp

test_pad_reconcile: test_pad_reconcile.cpp ../src/input/pad_reconcile.cpp ../src/input/pad_reconcile.h
	$(CXX) $(CXXFLAGS) -o $@ test_pad_reconcile.cpp ../src/input/pad_reconcile.cpp

test_hud: test_hud.cpp ../src/ui/hud.cpp ../src/ui/hud.h
	$(CXX) $(CXXFLAGS) -o $@ test_hud.cpp ../src/ui/hud.cpp

test_hotkey: test_hotkey.cpp ../src/input/hotkey.cpp ../src/input/hotkey.h ../src/input/joypad_map.cpp
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_hotkey.cpp ../src/input/hotkey.cpp ../src/input/joypad_map.cpp

test_splash: test_splash.cpp ../src/video/splash.cpp ../src/video/splash.h
	$(CXX) $(CXXFLAGS) -o $@ test_splash.cpp ../src/video/splash.cpp

test_controller_map: test_controller_map.cpp ../src/input/controller_map.cpp ../src/input/controller_map.h ../src/input/joypad_map.cpp
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_controller_map.cpp ../src/input/controller_map.cpp ../src/input/joypad_map.cpp

test_sega_board: test_sega_board.cpp ../src/input/sega_board.cpp ../src/input/sega_board.h
	$(CXX) $(CXXFLAGS) -o $@ test_sega_board.cpp ../src/input/sega_board.cpp

test_sega_pad: test_sega_pad.cpp ../src/input/sega_pad.cpp ../src/input/sega_pad.h ../src/input/joypad_map.h
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_sega_pad.cpp ../src/input/sega_pad.cpp

clean:
	rm -f test_blit test_pixel_ops test_glyph_draw test_icons test_theme test_fonts test_rom_layout test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings test_audio_util test_pad_reconcile test_hud test_hotkey test_splash test_controller_map test_sega_board test_sega_pad test_input_merge
