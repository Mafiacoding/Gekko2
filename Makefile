#---------------------------------------------------------------------------------
# Gekko2 - experimental PS2 emulator for Nintendo Wii
# Requires devkitPPC + libogc (DEVKITPRO / DEVKITPPC env vars must be set)
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
TARGET		:=	Gekko2
BUILD		:=	build
# Default CPU mode matches R1330H; native blocks can be selected in the
# launcher. build_r1331.sh provides an options build with blocks enabled.
EXTRA_CFLAGS ?=
SOURCES		:=	source source/core source/core/ee source/core/iop source/core/vu \
			source/core/gs source/core/recompiler source/hw
INCLUDES	:=	include
SOURCES += third_party/libchdr/src third_party/libchdr/deps/lzma-25.01/src third_party/libchdr/deps/miniz-3.1.1 third_party/libchdr/deps/zstd-1.5.7
INCLUDES += third_party/libchdr/include
CHD_FLAGS := -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1
SOURCES += third_party/wiidrc
INCLUDES += third_party/wiidrc
ifeq ($(LIGHTREC_PILOT),1)
override EXTRA_CFLAGS += -DGEKKO2_IOP_LIGHTREC_PILOT -DHAVE_MMAP=0 -DLOG_LEVEL=0
INCLUDES += third_party/lightrec third_party/lightrec/tlsf third_party/lightning/include
LIGHTREC_DIRS := third_party/lightrec third_party/lightrec/tlsf third_party/lightning/lib
LIGHTREC_FILES := blockcache.c constprop.c emitter.c interpreter.c lightrec.c memmanager.c optimizer.c regcache.c tlsf.c jit_disasm.c jit_memory.c jit_names.c jit_note.c jit_print.c jit_size.c lightning.c
endif

#---------------------------------------------------------------------------------
CFLAGS = -g -O1 -Wall $(MACHDEP) $(INCLUDE) -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float $(EXTRA_CFLAGS) $(CHD_FLAGS)
CXXFLAGS = $(CFLAGS) -std=gnu++17
ASFLAGS = $(CFLAGS)
LDFLAGS	=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map -Wl,-u,gs_gx_capture_vram_psmct32 -Wl,-u,gs_mem_gpu_pending -Wl,-u,gs_gx_draw_flat -Wl,-u,gs_mem_get -Wl,-u,ee_jit_try_execute_block -Wl,-u,ee_jit_reset_stats_for_test -Wl,-u,vu_micro_step_pipeline

LIBS	:= -lfat -lwiiuse -lbte -logc -lm

LIBDIRS	:= $(PORTLIBS) $(DEVKITPRO)/libogc

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir)) $(foreach dir,$(LIGHTREC_DIRS),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CFILES += $(LIGHTREC_FILES)
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			-I$(CURDIR)/source \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	-L$(LIBOGC_LIB) $(foreach dir,$(PORTLIBS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).dol

run:
	wiiload $(TARGET).dol

#---------------------------------------------------------------------------------
else
DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
	@echo output ... $(notdir $@)
	@if command -v elf2dol >/dev/null 2>&1; then \
		elf2dol $< $@ ; \
	else \
		cc -O2 -o $(TOPDIR)/tools/elf2dol_native $(TOPDIR)/tools/elf2dol.c 2>/dev/null ; \
		if [ -x $(TOPDIR)/tools/elf2dol_native ]; then \
			$(TOPDIR)/tools/elf2dol_native $< $@ ; \
		else \
			python3 $(TOPDIR)/tools/elf2dol.py $< $@ ; \
		fi ; \
	fi
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
