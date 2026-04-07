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

# M1/M2: stub kernel only.
# Genesis-Plus-GX-Wide objects are added in M4.
OBJS = src/main.o \
       src/kernel.o

# Libraries — most specific first, Circle core last.
# Each is built on demand by the targets below.
LIBS = $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/sound/libsound.a \
       $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN = src/*.o src/*.d

include $(CIRCLEHOME)/Rules.mk

# ---------------------------------------------------------------------------
# Circle library build targets
# Each sub-library is built on demand when first needed by the linker.
# Pass the same AARCH / RASPPI / PREFIX so all objects match the target ABI.
# ---------------------------------------------------------------------------

CIRCLE_MAKE = $(MAKE) -C $(@D) AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

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
