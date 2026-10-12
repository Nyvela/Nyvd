CC := gcc
LD := ld
OBJCOPY := objcopy

BUILD := build

TARGET := $(BUILD)/nyvd
ELF := $(TARGET).elf
BIN := $(TARGET).bin

USER_CFLAGS ?=
CFLAGS := -ffreestanding -m64 -mno-red-zone \
          -fno-stack-protector -fno-pie -fno-pic \
          -ffunction-sections -fdata-sections \
          -Wall -Wextra \
          -Iinclude

CFLAGS += $(USER_CFLAGS)

LDFLAGS := -nostdlib -static -Wl,--gc-sections -T linker.ld
SRC := $(shell find src -type f -name '*.c')

.PHONY: all clean

all: $(ELF) $(BIN)

$(ELF): $(SRC) linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary --set-section-flags .bss=alloc,load,contents $< $@

clean:
	rm -rf $(BUILD)
