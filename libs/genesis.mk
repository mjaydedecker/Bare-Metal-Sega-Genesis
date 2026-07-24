#
# libs/genesis.mk
#
# Bare Metal Sega Genesis
# Standalone Makefile for building libs/libgenesis.a from
# Genesis-Plus-GX-Wide (libretro port) + our bare-metal stubs.
#
# Invoked from the top-level Makefile:
#   $(MAKE) -f libs/genesis.mk PREFIX=$(PREFIX)
#
# All objects land in build/genesis/ mirroring the source tree so the
# submodule directory is never written to.
#

PREFIX   ?= arm-linux-gnueabihf-
CC        = $(PREFIX)gcc
AR        = $(PREFIX)ar

AARCH    ?= 32
RASPPI   ?= 2

GENESIS  = libs/genesis-plus-gx-wide
COMM     = $(GENESIS)/libretro/libretro-common
BDIR     = build/genesis

# ---------------------------------------------------------------------------
# Source directories (mirrors Makefile.common GENPLUS_SRC_DIR + tremor)
# ---------------------------------------------------------------------------
CORE_DIRS = \
    $(GENESIS)/core \
    $(GENESIS)/core/z80 \
    $(GENESIS)/core/m68k \
    $(GENESIS)/core/ntsc \
    $(GENESIS)/core/sound \
    $(GENESIS)/core/input_hw \
    $(GENESIS)/core/cd_hw \
    $(GENESIS)/core/cart_hw \
    $(GENESIS)/core/cart_hw/svp \
    $(GENESIS)/core/tremor

SOURCES_C  = $(foreach d,$(CORE_DIRS),$(wildcard $(d)/*.c))
SOURCES_C += $(GENESIS)/libretro/libretro.c
SOURCES_C += src/libretro/stubs.c

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
INCFLAGS  = $(foreach d,$(CORE_DIRS),-I$(d))
INCFLAGS += -I$(GENESIS)/libretro
INCFLAGS += -I$(COMM)/include

# ---------------------------------------------------------------------------
# Per-board CPU flags — mirrors Circle's Rules.mk ARCHCPU cases for AArch32
# (see libs/circle/Rules.mk) so this core always matches the -mcpu/-mfpu
# Circle itself uses for that board. RASPPI=5 (Pi 5) has no AArch32 target
# in Circle at all — it's AArch64-only — and AArch64 needs its own CFLAGS
# rework here (drops -marm/-mfpu/-mfloat-abi). Both unsupported for now.
# ---------------------------------------------------------------------------
ifneq ($(strip $(AARCH)),32)
$(error libs/genesis.mk only supports AARCH=32 currently (got AARCH=$(AARCH)))
endif

ifeq ($(strip $(RASPPI)),2)
ARCHCPU = -mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard
else ifeq ($(strip $(RASPPI)),3)
ARCHCPU = -mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else ifeq ($(strip $(RASPPI)),4)
ARCHCPU = -mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else
$(error libs/genesis.mk only supports RASPPI=2, 3 or 4 currently (got RASPPI=$(RASPPI)))
endif

# ---------------------------------------------------------------------------
# Compile flags — same ABI as Circle for the selected board
# ---------------------------------------------------------------------------
CFLAGS = \
    $(ARCHCPU) \
    -O2 -std=gnu99 -fsigned-char \
    -U_FORTIFY_SOURCE \
    "-DINLINE=static __inline__" \
    -DLSB_FIRST \
    "-DBYTE_ORDER=LITTLE_ENDIAN" \
    -DARM \
    -DALIGN_LONG \
    -DFRONTEND_SUPPORTS_RGB565=1 \
    -DUSE_16BPP_RENDERING \
    "-Dalloca=__builtin_alloca" \
    -DUSE_PER_SOUND_CHANNELS_CONFIG=1 \
    -DMAX_ROM_SIZE=10485760 \
    -ULOGSOUND \
    -DSTATIC_LINKING=1 \
    -DHAVE_STRL \
    -D__LIBRETRO__ \
    $(INCFLAGS)

# ---------------------------------------------------------------------------
# Object list — preserves source-relative path under $(BDIR)/
# ---------------------------------------------------------------------------
OBJS = $(patsubst %.c,$(BDIR)/%.o,$(SOURCES_C))

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all clean

all: libs/libgenesis.a

libs/libgenesis.a: $(OBJS)
	$(AR) rcs $@ $^

$(BDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BDIR) libs/libgenesis.a
