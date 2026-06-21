# Host-side unit tests (system compiler, not the cross toolchain).
CXX      ?= c++
CXXFLAGS ?= -Wall -Wextra -O2

LIBRETRO_INC = ../libs/genesis-plus-gx-wide/libretro/libretro-common/include

.PHONY: run clean

run: test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings test_audio_util test_pad_reconcile test_hud
	./test_blit
	./test_joypad
	./test_rom_filter
	./test_menu_state
	./test_menu_path
	./test_save_path
	./test_settings
	./test_audio_util
	./test_pad_reconcile
	./test_hud

test_blit: test_blit.cpp ../src/video/blit.cpp ../src/video/blit.h
	$(CXX) $(CXXFLAGS) -o $@ test_blit.cpp ../src/video/blit.cpp

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

clean:
	rm -f test_blit test_joypad test_rom_filter test_menu_state test_menu_path test_save_path test_settings test_audio_util test_pad_reconcile test_hud
