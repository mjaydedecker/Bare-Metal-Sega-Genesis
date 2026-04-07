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

# M1: stub kernel only.
# Genesis-Plus-GX-Wide objects are added in M4.
OBJS = src/main.o \
       src/kernel.o

# M1: only the Circle core library is needed.
# USB, scheduler, and FS libs are added as subsystems come online.
LIBS = $(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN = src/*.o src/*.d

include $(CIRCLEHOME)/Rules.mk

# Build the Circle core library before linking our kernel.
# Pass the same toolchain and target settings so it uses the
# same compiler and produces compatible object files.
$(CIRCLEHOME)/lib/libcircle.a:
	$(MAKE) -C $(CIRCLEHOME)/lib \
		AARCH=$(AARCH) RASPPI=$(RASPPI) PREFIX=$(PREFIX)

-include $(DEPS)
