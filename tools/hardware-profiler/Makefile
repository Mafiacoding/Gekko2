ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC must point to devkitPPC)
endif
include $(DEVKITPPC)/wii_rules
TARGET := gekko2-profiler
BUILD := build
SOURCES := source
INCLUDES :=
CFLAGS := -O2 -Wall -Wextra $(MACHDEP) $(INCLUDE) -DGEKKO
LDFLAGS := $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS := -lfat -logc -lm
LIBDIRS := $(LIBOGC_LIB)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export OFILES := $(addsuffix .o,$(basename $(notdir $(wildcard $(foreach dir,$(SOURCES),$(dir)/*.c)))))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
.PHONY: all clean
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
	@mkdir -p $@
clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).dol $(TARGET).map
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
