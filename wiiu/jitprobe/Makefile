# GPL-3.0-or-later. Wii U feasibility probe. Needs DEVKITPRO, DEVKITPPC and wut (see ../README.md).
ifeq ($(strip $(DEVKITPPC)),)
$(error Please set DEVKITPPC (and DEVKITPRO) in your environment)
endif
include $(DEVKITPRO)/wut/share/wut_rules
TARGET   := gekko2-wiiu-jitprobe
BUILD    := build
SOURCES  := source
APP_NAME := Gekko2 Wii U probe
APP_SHORTNAME := G2probe
APP_AUTHOR := Gekko2
CFLAGS   := -g -O2 -Wall $(MACHDEP) $(INCLUDE)
LDFLAGS  := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)
LIBS     := -lwut
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH  := $(foreach d,$(SOURCES),$(CURDIR)/$(d))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES   := $(foreach d,$(SOURCES),$(notdir $(wildcard $(d)/*.c)))
export OFILES := $(CFILES:.c=.o)
export INCLUDE := $(foreach d,$(PORTLIBS),-I$(d)/include) -I$(WUT_ROOT)/include
export LIBPATHS := $(foreach d,$(PORTLIBS),-L$(d)/lib) -L$(WUT_ROOT)/lib
export LD := $(CC)
.PHONY: $(BUILD) clean
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -rf $(BUILD) $(TARGET).rpx $(TARGET).elf $(TARGET).wuhb
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
