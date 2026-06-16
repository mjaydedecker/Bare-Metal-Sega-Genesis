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
       src/storage/sdcard.o \
       src/libretro/environment.o \
       src/libretro/callbacks.o \
       src/runtime_stubs.o \
       src/cstdlib_stubs.o \
       src/stdlib_stubs.o

# Extra include path so src/kernel.cpp and src/libretro/*.cpp can find
# libretro.h without polluting the genesis-core compile flags.
EXTRAINCLUDE = \
    -I libs/genesis-plus-gx-wide/libretro/libretro-common/include \
    -I libs/genesis-plus-gx-wide/libretro

# Libraries — most specific first, Circle core last.
# Each is built on demand by the targets below.
LIBS = libs/libgenesis.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/sound/libsound.a \
       $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN = src/*.o src/*.d src/storage/*.o src/storage/*.d \
             src/libretro/*.o src/libretro/*.d \
             build/genesis libs/libgenesis.a

include $(CIRCLEHOME)/Rules.mk

# ---------------------------------------------------------------------------
# Circle library build targets
# Each sub-library is built on demand when first needed by the linker.
# Pass the same AARCH / RASPPI / PREFIX so all objects match the target ABI.
# ---------------------------------------------------------------------------

CIRCLE_MAKE = $(MAKE) -C $(@D) AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX) DEFINE="-DKERNEL_MAX_SIZE=0x1400000"

libs/libgenesis.a:
	$(MAKE) -f libs/genesis.mk PREFIX=$(PREFIX)

$(CIRCLEHOME)/lib/libcircle.a:
	$(MAKE) -C $(CIRCLEHOME)/lib AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

$(CIRCLEHOME)/lib/usb/libusb.a:
	$(MAKE) -C $(CIRCLEHOME)/lib/usb AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

$(CIRCLEHOME)/lib/input/libinput.a:
	$(MAKE) -C $(CIRCLEHOME)/lib/input AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

$(CIRCLEHOME)/lib/sound/libsound.a:
	$(MAKE) -C $(CIRCLEHOME)/lib/sound AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

$(CIRCLEHOME)/lib/fs/fat/libfatfs.a:
	$(MAKE) -C $(CIRCLEHOME)/lib/fs/fat AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

$(CIRCLEHOME)/lib/fs/libfs.a:
	$(MAKE) -C $(CIRCLEHOME)/lib/fs AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

$(CIRCLEHOME)/addon/SDCard/libsdcard.a:
	$(MAKE) -C $(CIRCLEHOME)/addon/SDCard AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

-include $(DEPS)
