# Gekko2 portable-core bench for Wii (controlled experiment vs. the Wii U / Cemu port). Needs devkitPPC + libogc.
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC")
endif
include $(DEVKITPPC)/wii_rules
TARGET   := GekkoBench
BUILD    := build
OPT      ?= -O1
SOURCES  := . ../app ../../source ../../source/core ../../source/core/ee ../../source/core/iop ../../source/core/vu \
            ../../source/core/gs ../../source/core/recompiler ../../source/hw \
            ../../third_party/libchdr/src ../../third_party/libchdr/deps/lzma-25.01/src \
            ../../third_party/libchdr/deps/miniz-3.1.1 ../../third_party/libchdr/deps/zstd-1.5.7
INCLUDES := ../../third_party/wiidrc ../app ../../include ../../source ../../third_party/libchdr/include \
            ../../third_party/libchdr/deps/lzma-25.01/include ../../third_party/libchdr/deps/miniz-3.1.1 \
            ../../third_party/libchdr/deps/zstd-1.5.7 ../../third_party/wiidrc
EXCLUDE  := host_main.c ppc_dynarec.c wiiu_main.c
CHD_FLAGS := -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1
CFLAGS   = -g $(OPT) -w $(MACHDEP) -UGEKKO $(INCLUDE) -DGEKKO2_WII_BENCH $(CHD_FLAGS)
ASFLAGS  = $(CFLAGS)
LDFLAGS  = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS     := -lfat -lwiiuse -lbte -logc -lm
LIBDIRS  := $(PORTLIBS) $(DEVKITPRO)/libogc
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES  := $(filter-out $(EXCLUDE),$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))))
export OFILES := $(CFILES:.c=.o)
export LD := $(CC)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := -L$(LIBOGC_LIB) $(foreach dir,$(PORTLIBS),-L$(dir)/lib)
.PHONY: $(BUILD) clean
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).dol
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
