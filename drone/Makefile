# Build the M1 two-PD image using the seL4 Microkit SDK.
#
# Requires:
#   MICROKIT_SDK   path to an extracted Microkit SDK release
#   TOOLCHAIN      AArch64 bare-metal toolchain prefix (default aarch64-none-elf)
#
# Usage:
#   make                    # build image for qemu_virt_aarch64
#   make qemu               # boot the image under QEMU
#   make BOARD=odroidc4     # cross-build for real hardware
#   make clean

ifndef MICROKIT_SDK
$(error MICROKIT_SDK is not set. Point it at your extracted Microkit SDK release.)
endif

BOARD     ?= qemu_virt_aarch64
CONFIG    ?= debug
BUILD_DIR ?= build
TOOLCHAIN ?= aarch64-none-elf

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld

BOARD_DIR := $(MICROKIT_SDK)/board/$(BOARD)/$(CONFIG)
MICROKIT  := $(MICROKIT_SDK)/bin/microkit

CFLAGS := -mcpu=cortex-a53 -mstrict-align -ffreestanding -g3 -O2 \
          -Wall -Wextra -Wno-unused-parameter \
          -I$(BOARD_DIR)/include
LDFLAGS := -L$(BOARD_DIR)/lib
LIBS    := -lmicrokit -Tmicrokit.ld

PDS       := pinger ponger
ELFS      := $(PDS:%=$(BUILD_DIR)/%.elf)
IMAGE     := $(BUILD_DIR)/drone.img
REPORT    := $(BUILD_DIR)/report.txt

.PHONY: all qemu clean

all: $(IMAGE)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.elf: $(BUILD_DIR)/%.o
	$(LD) $(LDFLAGS) $< $(LIBS) -o $@

$(IMAGE) $(REPORT): board.system $(ELFS)
	$(MICROKIT) board.system \
	    --search-path $(BUILD_DIR) \
	    --board $(BOARD) \
	    --config $(CONFIG) \
	    -o $(IMAGE) \
	    -r $(REPORT)

qemu: $(IMAGE)
	qemu-system-aarch64 \
	    -machine virt,virtualization=on \
	    -cpu cortex-a53 \
	    -m 2G \
	    -nographic \
	    -serial mon:stdio \
	    -device loader,file=$(IMAGE),addr=0x70000000,cpu-num=0

clean:
	rm -rf $(BUILD_DIR)
