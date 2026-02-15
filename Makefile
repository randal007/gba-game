#---------------------------------------------------------------------------------
# GBA Isometric Action Game - Makefile
#---------------------------------------------------------------------------------

export DEVKITPRO := /opt/devkitpro
export DEVKITARM := $(DEVKITPRO)/devkitARM
include $(DEVKITARM)/gba_rules

#---------------------------------------------------------------------------------
# Project settings
#---------------------------------------------------------------------------------
TARGET   := isogame
BUILD    := build
SOURCES  := src data
INCLUDES := include
DATA     := data

#---------------------------------------------------------------------------------
# Tools
#---------------------------------------------------------------------------------
CC       := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY  := $(DEVKITARM)/bin/arm-none-eabi-objcopy

#---------------------------------------------------------------------------------
# Flags
#---------------------------------------------------------------------------------
ARCH     := -mthumb -mthumb-interwork
CFLAGS   := -g -Wall -O2 -mcpu=arm7tdmi -mtune=arm7tdmi $(ARCH) -DGBA
CFLAGS   += -I$(DEVKITPRO)/libtonc/include -I$(INCLUDES)
LDFLAGS  := -g $(ARCH) -specs=gba.specs
LIBS     := -L$(DEVKITPRO)/libtonc/lib -ltonc

#---------------------------------------------------------------------------------
# File lists
#---------------------------------------------------------------------------------
CFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES   := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(CFILES)))
DFILES   := $(OFILES:.o=.d)
VPATH    := $(SOURCES)

#---------------------------------------------------------------------------------
# Rules
#---------------------------------------------------------------------------------
.PHONY: all clean run

all: $(BUILD) $(TARGET).gba

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "==> Built $(TARGET).gba"
	gbafix $@

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(BUILD):
	@mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba

run: $(TARGET).gba
	mgba-qt $<

-include $(DFILES)
