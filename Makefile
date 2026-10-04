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
SOURCES		:=	source source/core source/core/ee source/core/iop source/core/vu \
			source/core/gs source/core/recompiler source/hw
INCLUDES	:=	include

#---------------------------------------------------------------------------------
CFLAGS	= -g -O3 -Wall $(MACHDEP) $(INCLUDE) -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float $(EXTRA_CFLAGS)
CXXFLAGS = $(CFLAGS) -std=gnu++17
LDFLAGS	=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map -Wl,-u,gs_gx_capture_vram_psmct32 -Wl,-u,gs_mem_gpu_pending -Wl,-u,gs_gx_draw_flat -Wl,-u,gs_mem_get -Wl,-u,ee_jit_try_execute_block -Wl,-u,ee_jit_reset_stats_for_test

LIBS	:= -lfat -lwiiuse -lbte -logc -lm

LIBDIRS	:= $(PORTLIBS) $(DEVKITPRO)/libogc

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
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
