# Host-side unit tests (system compiler, not the cross toolchain).
CXX      ?= c++
CXXFLAGS ?= -Wall -Wextra -O2

LIBRETRO_INC = ../libs/genesis-plus-gx-wide/libretro/libretro-common/include

.PHONY: run clean

run: test_blit test_joypad test_rom_filter
	./test_blit
	./test_joypad
	./test_rom_filter

test_blit: test_blit.cpp ../src/video/blit.cpp ../src/video/blit.h
	$(CXX) $(CXXFLAGS) -o $@ test_blit.cpp ../src/video/blit.cpp

test_joypad: test_joypad.cpp ../src/input/joypad_map.cpp ../src/input/joypad_map.h
	$(CXX) $(CXXFLAGS) -I$(LIBRETRO_INC) -o $@ test_joypad.cpp ../src/input/joypad_map.cpp

test_rom_filter: test_rom_filter.cpp ../src/menu/rom_filter.cpp ../src/menu/rom_filter.h
	$(CXX) $(CXXFLAGS) -o $@ test_rom_filter.cpp ../src/menu/rom_filter.cpp

clean:
	rm -f test_blit test_joypad test_rom_filter test_menu_state test_menu_path
