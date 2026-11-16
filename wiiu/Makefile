# GPL-3.0-or-later. Gekko2 Wii U / Cemu test port. Run from this directory:
#   export DEVKITPRO=/opt/devkitpro DEVKITPPC=$DEVKITPRO/devkitPPC; make
# Produces gekko2-wiiu.rpx (Cemu: File > Load) and gekko2-wiiu.wuhb. The portable Gekko2 core is built
# in "host" mode (no GEKKO/libogc, no AOT/JIT): interpreter only. Third-party sources are compiled
# unchanged from ../third_party.
ifeq ($(strip $(DEVKITPPC)),)
$(error Please set DEVKITPRO and DEVKITPPC in your environment)
endif
include $(DEVKITPRO)/wut/share/wut_rules

TARGET        := gekko2-wiiu
BUILD         := build
APP_NAME      := Gekko2 (Wii U test)
APP_SHORTNAME := Gekko2
APP_AUTHOR    := Gekko2
ROOT          := $(abspath $(CURDIR)/..)
SOURCES       := app ../source ../source/core ../source/core/ee ../source/core/iop ../source/core/vu \
                 ../source/core/gs ../source/core/recompiler ../source/hw \
                 ../third_party/libchdr/src ../third_party/libchdr/deps/lzma-25.01/src \
                 ../third_party/libchdr/deps/miniz-3.1.1 ../third_party/libchdr/deps/zstd-1.5.7
INCLUDES      := ../include ../source ../third_party/libchdr/include ../third_party/libchdr/deps/lzma-25.01/include \
                 ../third_party/libchdr/deps/miniz-3.1.1 ../third_party/libchdr/deps/zstd-1.5.7 app
CHD_FLAGS     := -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_DEFLATE_APIS \
                 -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1
CFLAGS        := -g -O2 -w $(MACHDEP) $(INCLUDE) -DGEKKO2_WIIU $(CHD_FLAGS)
LDFLAGS       := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)
LIBS          := -lwut -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach d,$(SOURCES),$(CURDIR)/$(d))
export DEPSDIR := $(CURDIR)/$(BUILD)
EXCLUDE        := main.c ppc_dynarec.c host_main.c
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
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
