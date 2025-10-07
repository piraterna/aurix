###################################################################################
## Module Name:  Makefile                                                        ##
## Project:      AurixOS                                                         ##
##                                                                               ##
## Copyright (c) 2024 Jozef Nagy                                                 ##
##                                                                               ##
## This source is subject to the MIT License.                                    ##
## See License.txt in the root of this repository.                               ##
## All other rights reserved.                                                    ##
##                                                                               ##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    ##
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      ##
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   ##
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        ##
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, ##
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE ##
## SOFTWARE.                                                                     ##
###################################################################################

.DEFAULT_GOAL := all

INCLUDE_DIRS := include

BUILD_DIR ?= build
SYSROOT_DIR ?= sysroot

KERNEL_FILE := $(BUILD_DIR)/axkrnl

INCLUDE_DIRS := include \
				include/lib \
				include/ext \
				include/arch/$(ARCH) \
				include/platform/$(PLATFORM)
DEFINES := __$(ARCH)__

KERNEL_NASM := nasm
KERNEL_HOSTTOOLCHAIN ?= 0

ifeq ($(KERNEL_HOSTTOOLCHAIN),0)
KERNEL_AS      := $(ARCH)-elf-gcc
KERNEL_CC      := $(ARCH)-elf-gcc
KERNEL_LD      := $(ARCH)-elf-ld
KERNEL_OBJCOPY := $(ARCH)-elf-objcopy
else
KERNEL_AS      := gcc
KERNEL_CC      := gcc
KERNEL_LD      := ld
KERNEL_OBJCOPY := objcopy
endif

KERNEL_ASFLAGS := $(foreach d, $(INCLUDE_DIRS), -I$d) \
				$(foreach d, $(DEFINES), -D$d)

KERNEL_NASMFLAGS := $(foreach d, $(INCLUDE_DIRS), -I$d) \
					$(foreach d, $(DEFINES), -D$d) \
					-felf64 \
					-gdwarf

KERNEL_CFLAGS := $(foreach d, $(INCLUDE_DIRS), -I$d) \
				$(foreach d, $(DEFINES), -D$d) \
				-D__$(ARCH) \
				-std=c99 \
				-Wall \
				-Wextra \
				-Werror \
				-Wno-unused-function \
				-ffreestanding \
				-fno-omit-frame-pointer \
				-fno-stack-protector \
				-fno-stack-check \
				-fno-pic \
				-fno-pie \
				-MMD \
				-MP

KERNEL_LDFLAGS := -Tarch/$(ARCH)/linker.ld \
				-nostdlib \
				-static

ifeq ($(BUILD_TYPE),debug)
KERNEL_CFLAGS += -O0 -g3 -D_DEBUG
else
KERNEL_CFLAGS += -O2
endif

KERNEL_CFILES := $(shell find . -type d \( -name arch -o -name platform \) -prune -o -name '*.c' -print)
KERNEL_ARCH_CFILES := $(shell find arch/$(ARCH) -name '*.c')
KERNEL_ARCH_ASFILES := $(shell find arch/$(ARCH) -name '*.S')
KERNEL_ARCH_NASMFILES := $(shell find arch/$(ARCH) -name '*.asm')
KERNEL_PLATFORM_CFILES := $(shell find platform/$(PLATFORM) -name '*.c')
KERNEL_PLATFORM_ASFILES := $(shell find platform/$(PLATFORM) -name '*.S')
KERNEL_PLATFORM_NASMFILES := $(shell find platform/$(PLATFORM) -name '*.asm')

KERNEL_OBJ := $(KERNEL_CFILES:%.c=$(BUILD_DIR)/kernel/%.c.o) \
			$(KERNEL_ARCH_CFILES:arch/$(ARCH)/%.c=$(BUILD_DIR)/kernel/arch/%.c.o) \
			$(KERNEL_ARCH_ASFILES:arch/$(ARCH)/%.S=$(BUILD_DIR)/kernel/arch/%.S.o) \
			$(KERNEL_ARCH_NASMFILES:arch/$(ARCH)/%.asm=$(BUILD_DIR)/kernel/arch/%.asm.o) \
			$(KERNEL_PLATFORM_CFILES:platform/$(PLATFORM)/%.c=$(BUILD_DIR)/kernel/platform/%.c.o) \
			$(KERNEL_PLATFORM_NASMFILES:platform/$(PLATFORM)/%.asm=$(BUILD_DIR)/kernel/platform/%.asm.o)

-include arch/$(ARCH)/config.mk

-include $(wildcard $(BUILD_DIR)/kernel/*.d)

$(KERNEL_FILE): $(KERNEL_OBJ)
	@mkdir -p $(@D)
	@printf "  LD\t$(notdir $@)\n"
	@$(KERNEL_LD) $(KERNEL_LDFLAGS) $^ -o $@
ifneq ($(BUILD_TYPE),debug)
	@printf "  OBJCOPY axkrnl.sym\n"
	@$(KERNEL_OBJCOPY) --only-keep-debug $@ $(BUILD_DIR)/axkrnl.sym
	@$(KERNEL_OBJCOPY) --strip-debug --strip-unneeded $@
endif

$(BUILD_DIR)/kernel/%.c.o: %.c
	@mkdir -p $(@D)
	@printf "  CC\t$<\n"
	@$(KERNEL_CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/arch/%.c.o: arch/$(ARCH)/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$<\n"
	@$(KERNEL_CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/arch/%.S.o: arch/$(ARCH)/%.S
	@mkdir -p $(@D)
	@printf "  AS\t$<\n"
	@$(KERNEL_AS) $(KERNEL_ASFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/arch/%.asm.o: arch/$(ARCH)/%.asm
	@mkdir -p $(@D)
	@printf "  NASM\t$<\n"
	@$(KERNEL_NASM) $(KERNEL_NASMFLAGS) $< -o $@

$(BUILD_DIR)/kernel/platform/%.c.o: platform/$(PLATFORM)/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$<\n"
	@$(KERNEL_CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/platform/%.S.o: platform/$(PLATFORM)/%.S
	@mkdir -p $(@D)
	@printf "  AS\t$<\n"
	@$(KERNEL_AS) $(KERNEL_ASFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/platform/%.asm.o: platform/$(PLATFORM)/%.asm
	@mkdir -p $(@D)
	@printf "  NASM\t$<\n"
	@$(KERNEL_NASM) $(KERNEL_NASMFLAGS) $< -o $@

.PHONY: all
all: kernel

.PHONY: kernel
kernel: $(KERNEL_FILE)

.PHONY: install
install: install-kernel

.PHONY: install-kernel
install-kernel: kernel
	@mkdir -p $(SYSROOT_DIR)/System
	@printf "  INSTALL\t/System/$(notdir $(KERNEL_FILE))\n"
	@cp $(KERNEL_FILE) $(SYSROOT_DIR)/System

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR) $(SYSROOT_DIR)
