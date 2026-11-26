# GPL-3.0-or-later. Gekko2 Wii U / Cemu test port. Run from this directory:
#   export DEVKITPRO=/opt/devkitpro DEVKITPPC=$DEVKITPRO/devkitPPC; make
# Produces gekko2-wiiu.rpx (Cemu: File > Load) and gekko2-wiiu.wuhb. The portable Gekko2 core is built
# in "host" mode (no GEKKO/libogc, no AOT): interpreter only, or with `make JIT=1` the EE dynarec in the
# Wii U codegen area (experimental, see app/wiiu_main.c). Third-party sources are compiled
# unchanged from ../third_party.
ifeq ($(strip $(DEVKITPPC)),)
$(error Please set DEVKITPRO and DEVKITPPC in your environment)
endif
include $(DEVKITPRO)/wut/share/wut_rules

JIT        ?= 0
SPEED_STUB ?= 0
ifeq ($(JIT),1)
TARGET   := gekko2-wiiu-jit
BUILD    := build-jit
# Wii U provides a 16 MiB codegen area; use 15 MiB instead of the 6 MiB Wii default
# to avoid eviction/codegen-switch thrashing during heavy code phases.
JITFLAGS := -DGEKKO2_WIIU_JIT -DPPC_CODE_CACHE_BYTES=15728640u
ifeq ($(SPEED_STUB),1)
JITFLAGS += -DGEKKO2_SPEED_STUB
endif
else
TARGET        := gekko2-wiiu
BUILD         := build
JITFLAGS      := -DGEKKO2_WIIU_GX2
endif
ifeq ($(JIT),1)
APP_NAME      := Gekko2 Wii U Edition JIT (Alpha)
APP_SHORTNAME := Gekko2 JIT
else
APP_NAME      := Gekko2 Wii U Edition (Alpha)
APP_SHORTNAME := Gekko2
endif
APP_AUTHOR    := Gekko2
ROOT          := $(abspath $(CURDIR)/..)
SOURCES       := app gx2/src ../source ../source/core ../source/core/ee ../source/core/iop ../source/core/vu \
                 ../source/core/gs ../source/core/recompiler ../source/hw \
                 ../third_party/libchdr/src ../third_party/libchdr/deps/lzma-25.01/src \
                 ../third_party/libchdr/deps/miniz-3.1.1 ../third_party/libchdr/deps/zstd-1.5.7
INCLUDES      := gx2/src ../include ../source ../third_party/libchdr/include ../third_party/libchdr/deps/lzma-25.01/include \
                 ../third_party/libchdr/deps/miniz-3.1.1 ../third_party/libchdr/deps/zstd-1.5.7 app
CHD_FLAGS     := -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_DEFLATE_APIS \
                 -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1
OPT           ?= -O1
CFLAGS        := -g $(OPT) -w $(MACHDEP) $(INCLUDE) -DGEKKO2_WIIU $(JITFLAGS) $(CHD_FLAGS) $(EXTRA_CFLAGS)
LDFLAGS       := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)
LIBS          := -lwut -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach d,$(SOURCES),$(CURDIR)/$(d))
export DEPSDIR := $(CURDIR)/$(BUILD)
ifeq ($(JIT),1)
EXCLUDE        := main.c host_main.c
else
EXCLUDE        := main.c ppc_dynarec.c host_main.c
endif
CFILES         := $(filter-out $(EXCLUDE),$(foreach d,$(SOURCES),$(notdir $(wildcard $(d)/*.c))))
export OFILES  := $(CFILES:.c=.o)
export INCLUDE := $(foreach d,$(INCLUDES),-I$(CURDIR)/$(d)) $(foreach d,$(PORTLIBS),-I$(d)/include) -I$(WUT_ROOT)/include
export LIBPATHS := $(foreach d,$(PORTLIBS),-L$(d)/lib) -L$(WUT_ROOT)/lib
export LD      := $(CC)
.PHONY: $(BUILD) clean wuhb
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
wuhb: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).wuhb
clean:
	@rm -rf $(BUILD) $(TARGET).rpx $(TARGET).elf $(TARGET).wuhb
else
# O2_FILES: source file names (e.g. ee_core.c) built with -O2 while everything else keeps $(OPT); used to
# bisect the -O2 hang seen with devkitPPC GCC 16.2 (see README).
O2_FILES ?=
$(foreach f,$(O2_FILES),$(eval $(basename $(f)).o: CFLAGS += -O2 $(O2_EXTRA)))
# O1_FILES: built with -O1 even when OPT=-O2; O2_EXTRA: extra flags for the O2_FILES (flag bisecting).
O1_FILES ?=
O2_EXTRA ?=
$(foreach f,$(O1_FILES),$(eval $(basename $(f)).o: CFLAGS += -O1))
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
