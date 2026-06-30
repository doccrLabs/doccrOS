#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (c) 2026 doccrLabs
#
# PROJECT: doccrOS
# FILE: common.mk
# CREATED BY: emex
# MODIFIED BY: --
#
#

OS_NAME ?= doccrOS
ARCH ?= x86_64

# Build toolchain
CC := $(ARCH)-elf-gcc
CXX := $(ARCH)-elf-g++
LD := $(ARCH)-elf-ld
AS := nasm
OBJCOPY := $(ARCH)-elf-objcopy
VCC  = @echo "   CC   $<" && $(CC)
VCXX = @echo "   CXX  $<" && $(CXX)
VAS  = @echo "   AS   $<" && $(AS)
VLD  = @echo "   LD   $@" && $(LD)

# Compiler Flags
COMMON_FLAGS += -I $(INCLUDE_DIR) -I $(SRC_DIR) -I doccr/ -I doccr/kernel/ -ffreestanding -fno-stack-protector -fno-lto \
                -fno-PIE -fno-pic -m64 -march=x86-64 -mno-80387 -mno-mmx \
                -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -Wall -Wextra -Wpedantic
CFLAGS ?= $(COMMON_FLAGS) -std=c23
CXXFLAGS ?= $(COMMON_FLAGS) -std=c++17 -fno-exceptions -fno-rtti
LDFLAGS ?= -nostdlib -static -no-pie -z text -z max-page-size=0x1000
ASFLAGS ?= -f elf64

# Directories and files
SRC_DIR := doccr
USERSPACE_DIR = user
ARCH_DIR := doccr/kernel/arch/$(ARCH)
USERSPACE_BUILD = build/userspace
BUILD_DIR := build
DISK_DIR := dsk
DISK_IMG := $(DISK_DIR)/disk.img
INCLUDE_DIR := include
ISODIR := $(BUILD_DIR)/isodir
ISO := $(BUILD_DIR)/$(OS_NAME).iso
