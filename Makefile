#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (c) 2026 doccrLabs
#
# PROJECT: doccrOS
# FILE: Makefile
# CREATED BY: emex
# MODIFIED BY: --
#
#

include common.mk

LIMINE_DIR := $(INCLUDE_DIR)/limine
LIMINE_TOOL := $(LIMINE_DIR)/limine

# Find all C, C++ and Assembly files
SRCS = $(shell find $(SRC_DIR) -name "*.c" -or -name "*.cpp" -or -name "*.asm")
OBJS = $(SRCS:%=$(BUILD_DIR)/%.o)

.PHONY: all fetchDeps disk run clean
all: $(ISO)

# Fetch dependencies/libraries
fetchDeps:
	@echo "[DEPS] Fetching dependencies/libraries"
	@mkdir -p $(INCLUDE_DIR)

	@echo "[DEPS] Fetching Limine"
	@rm -rf $(LIMINE_DIR)
	@git clone https://codeberg.org/Limine/Limine.git --branch=v11.x-binary --depth=1 $(LIMINE_DIR)
	@rm -rf $(LIMINE_DIR)/.git
	@echo "[DEPS] Fetching Limine protocol header file"
	@wget https://codeberg.org/Limine/limine-protocol/raw/branch/trunk/include/limine.h -O $(LIMINE_DIR)/limine.h

disk:
	@mkdir -p $(DISK_DIR)
	@if [ -f $(DISK_IMG) ]; then \
		rm $(DISK_IMG); \
	fi
	@qemu-img create -f raw $(DISK_IMG) 256M
	@echo "[DISK] created $(DISK_IMG)"

# Kernel binary
$(BUILD_DIR)/kernel.elf: $(ARCH_DIR)/linker.ld $(OBJS)
	@mkdir -p $(dir $@)
	$(VLD) $(LDFLAGS) -T $< $(OBJS) -o $@

# Create bootable ISO
$(ISO): limine.conf $(BUILD_DIR)/kernel.elf disk
	@echo "[ISO] Creating bootable image..."
	@rm -rf $(ISODIR)
	@mkdir -p $(ISODIR)/boot/limine $(ISODIR)/EFI/BOOT
	@cp $(BUILD_DIR)/kernel.elf $(ISODIR)/boot/kernel_a.elf
	@cp $< $(ISODIR)/boot/limine/
	@cp $(addprefix $(INCLUDE_DIR)/limine/limine-, bios.sys bios-cd.bin uefi-cd.bin) $(ISODIR)/boot/limine/
	@cp $(addprefix $(INCLUDE_DIR)/limine/BOOT, IA32.EFI X64.EFI) $(ISODIR)/EFI/BOOT/
	@xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISODIR) -o $@ 2>/dev/null
	@echo "------------------------"
	@echo "[OK] $@ created"

# Run/Emulate OS
run: $(ISO)
	@echo "[QEMU]running $(OS_NAME).iso "
	@echo
	@qemu-system-x86_64 \
		-M pc \
		-cpu qemu64 \
		-m 1024M \
		-netdev user,id=net0 \
		-device e1000,netdev=net0 \
		-device AC97 \
		-drive if=pflash,format=raw,readonly=on,file=include/uefi/OVMF_CODE.fd \
		-drive if=pflash,format=raw,file=include/uefi/OVMF_VARS.fd \
		-drive file=$(DISK_IMG),format=raw,if=ide,index=0 \
		-cdrom $< \
		-serial stdio 2>&1 \
		#-no-reboot \
		#-no-shutdown

# Compilation rules
$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(VCC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(VCXX) $(CXXFLAGS) -c $< -o $@
$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(VAS) $(ASFLAGS) $< -o $@

# Clean all build output
clean:
	@echo "[CLR] Cleaning..."
	@rm -rf $(BUILD_DIR)
	@echo "[OK]"
