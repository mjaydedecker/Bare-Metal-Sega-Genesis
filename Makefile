#
# Makefile
#
# Bare Metal Sega Genesis
#

CIRCLEHOME = libs/circle

# Target: Raspberry Pi 2, AArch32
# Produces: kernel7.img
AARCH   = 32
RASPPI  = 2

# Override Circle's default (arm-none-eabi-) to use the
# arm-linux-gnueabihf toolchain available via apt on Ubuntu/Debian.
PREFIX  = arm-linux-gnueabihf-

# The emulation core's data+BSS extends to ~18 MB.  Circle's default
# KERNEL_MAX_SIZE is 2 MB, which would place the kernel stack at 0x228000 —
# inside our .data section — causing an instant crash on first function call.
# Override to 20 MB so MEM_KERNEL_STACK is placed safely above the BSS.
DEFINE += -DKERNEL_MAX_SIZE=0x1400000

# Genesis-Plus-GX-Wide objects are added in M4.
OBJS = src/main.o \
       src/kernel.o \
       src/libretro/environment.o \
       src/libretro/callbacks.o \
       src/runtime_stubs.o \
       src/cstdlib_stubs.o \
       src/stdlib_stubs.o \
       src/video/blit.o \
       src/video/display.o \
       src/audio/audio_driver.o \
       src/input/joypad_map.o \
       src/input/gamepad.o \
       src/storage/storage.o \
       src/menu/rom_filter.o \
       src/menu/menu_state.o \
       src/menu/menu_path.o \
       src/menu/rom_menu.o \
       src/menu/pause_menu.o \
       src/menu/save_path.o \
       src/menu/save_state.o \
       src/ui/text_canvas.o

# Extra include path so src/kernel.cpp and src/libretro/*.cpp can find
# libretro.h without polluting the genesis-core compile flags.
EXTRAINCLUDE = \
    -I libs/genesis-plus-gx-wide/libretro/libretro-common/include \
    -I libs/genesis-plus-gx-wide/libretro \
    -I libs/circle/addon/fatfs

# Libraries — most specific first, Circle core last.
# Each is built on demand by the targets below.
LIBS = libs/libgenesis.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/sound/libsound.a \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN = src/*.o src/*.d src/storage/*.o src/storage/*.d \
             src/menu/*.o src/menu/*.d \
             src/ui/*.o src/ui/*.d \
             src/libretro/*.o src/libretro/*.d \
             src/video/*.o src/video/*.d \
             src/audio/*.o src/audio/*.d \
             src/input/*.o src/input/*.d \
             build/genesis libs/libgenesis.a

include $(CIRCLEHOME)/Rules.mk

# ---------------------------------------------------------------------------
# Circle library build targets
# Each sub-library is built on demand when first needed by the linker.
# Pass the same AARCH / RASPPI / PREFIX so all objects match the target ABI.
#
# The KERNEL_MAX_SIZE override (see line ~22) MUST reach Circle's own
# startup/mem code, not just our app objects — otherwise Circle places the
# kernel stack at the 2 MB default (0x228000), inside our ~18 MB image, and
# the kernel self-corrupts before main().  A command-line DEFINE= would
# clobber Circle's own `DEFINE +=` in Rules.mk, so we inject it through
# Circle's Config.mk instead, which Rules.mk includes before those appends.
# Every Circle sub-library depends on that generated Config.mk.
# ---------------------------------------------------------------------------

$(CIRCLEHOME)/Config.mk:
	echo 'DEFINE += -DKERNEL_MAX_SIZE=0x1400000' > $@

CIRCLE_MAKE = $(MAKE) -C $(@D) AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

libs/libgenesis.a:
	$(MAKE) -f libs/genesis.mk PREFIX=$(PREFIX)

$(CIRCLEHOME)/lib/libcircle.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

$(CIRCLEHOME)/lib/usb/libusb.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

$(CIRCLEHOME)/lib/input/libinput.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

$(CIRCLEHOME)/lib/sound/libsound.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

$(CIRCLEHOME)/addon/SDCard/libsdcard.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

$(CIRCLEHOME)/addon/fatfs/libfatfs.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

# libfs.a provides CPartitionManager, used by CEMMCDevice (and USB mass
# storage) — required even though we no longer use the built-in CFATFileSystem.
$(CIRCLEHOME)/lib/fs/libfs.a: $(CIRCLEHOME)/Config.mk
	$(CIRCLE_MAKE)

-include $(DEPS)
