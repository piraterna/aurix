###################################################################################
## Module Name:  Makefile                                                        ##
## Project:      AurixOS                                                         ##
##                                                                               ##
## Copyright (c) 2024-2025 Jozef Nagy                                            ##
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

GITREV := $(shell git rev-parse --short HEAD)
export AURIXBUILD

##
# Kconfig configuration
#
export KCONFIG_CONFIG := .config
export MENUCONFIG_STYLE := aquatic

##
# Build configuration
#

export ARCH ?= x86_64
export PLATFORM ?= generic-pc
export BUILD_TYPE ?= debug

export ROOT_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

export BUILD_DIR ?= $(ROOT_DIR)/build
export SYSROOT_DIR ?= $(ROOT_DIR)/sysroot
export RELEASE_DIR ?= $(ROOT_DIR)/release

export NOUEFI ?= n

##
# Image generation and running
#

RAMDISK := $(BUILD_DIR)/ramdisk.gz

LIVECD := $(RELEASE_DIR)/aurix-$(GITREV)-livecd_$(ARCH)-$(PLATFORM).iso
LIVEHDD := $(RELEASE_DIR)/aurix-$(GITREV)-livehdd_$(ARCH)-$(PLATFORM).img
LIVESD := $(RELEASE_DIR)/aurix-$(GITREV)-livesd_$(ARCH)-$(PLATFORM).img

QEMU_FLAGS := -m 2G -smp 4 -rtc base=localtime -serial stdio

#QEMU_FLAGS += -device VGA -device qemu-xhci -device usb-kbd -device usb-mouse

# QEMU Audio support (macos only)
#QEMU_FLAGS += -audiodev coreaudio,id=coreaudio0 -device ich9-intel-hda -device hda-output,audiodev=coreaudio0

# QEMU Mouse support
#QEMU_FLAGS += -usb -device usb-mouse

-include machine/$(ARCH)/qemu.mk

##
# General info
#

export CODENAME := "Matterhorn"
export VERSION := "0.1"
export GITREV := $(shell git rev-parse --short HEAD)

export DEFINES := AURIX_CODENAME=$(CODENAME) \
				AURIX_VERSION=$(VERSION) \
				AURIX_GITREV=$(GITREV) \
				BUILD_TYPE=$(BUILD_TYPE)

ifeq ($(BUILD_TYPE),debug)
DEFINES += BUILD_DEBUG
else
DEFINES += BUILD_RELEASE
endif

##
# Recipes
#

.PHONY: all
all: genconfig boot kernel kmodules
	@:

.PHONY: boot
boot:
	@printf ">>> Building bootloader...\n"
	@$(MAKE) -C boot all

.PHONY: kernel
kernel:
	@printf ">>> Building kernel...\n"
	@$(MAKE) -C kernel

.PHONY: kmodules
kmodules:
	@$(MAKE) -C kernel/modules

.PHONY: install
install: boot kernel kmodules
	@printf ">>> Building sysroot...\n"
	@mkdir -p $(SYSROOT_DIR)
ifneq (,$(filter $(ARCH),i686 x86_64))
	@$(MAKE) -C boot install PLATFORM=pc-bios
else
	@$(MAKE) -C boot install
endif
ifneq (,$(filter $(ARCH),i686 x86_64 arm32 aarch64))
ifeq ($(NOUEFI),n)
	@$(MAKE) -C boot install PLATFORM=uefi
endif
endif
	@$(MAKE) -C kernel install
	@$(MAKE) -C kernel/modules install
	@printf "  GEN ramdisk\n"
	@mkdir -p $(SYSROOT_DIR)/ramdisk/modules
	@cp $(SYSROOT_DIR)/System/support/dummy.sys $(SYSROOT_DIR)/ramdisk/
	@utils/genramdisk.sh $(SYSROOT_DIR)/System/ramdisk

.PHONY: livecd
livecd: install
	@printf ">>> Generating Live CD..."
	@mkdir -p $(RELEASE_DIR)
	@utils/arch/$(ARCH)/generate-iso.sh $(LIVECD)

.PHONY: livehdd
livehdd: install
	@printf ">>> Generating Live HDD..."
	@mkdir -p $(RELEASE_DIR)
	@utils/arch/$(ARCH)/generate-hdd.sh $(LIVEHDD)

.PHONY: livesd
livesd: install
	@$(error SD Card Generation is not supported yet!)
	@printf ">>> Generating Live SD Card..."
	@mkdir -p $(RELEASE_DIR)
	@utils/arch/$(ARCH)/generate-sd.sh $(LIVESD)

.PHONY: run
run: livecd
	@printf ">>> Running QEMU...\n"
	@qemu-system-$(ARCH) $(QEMU_FLAGS) $(QEMU_MACHINE_FLAGS) -cdrom $(LIVECD)

nvram:
	@printf ">>> Generating NVRAM...\n"
	@./utils/gen-nvram.sh -o uefi_nvram.json -var,guid=d8637320-2230-4748-b8e8-a69d8e9708f6,name=boot-args,data="-v debug\0",attr=7

.PHONY: run-uefi
run-uefi: livecd nvram
	@printf ">>> Running QEMU (UEFI)...\n"
	@qemu-system-$(ARCH) $(QEMU_FLAGS) $(QEMU_MACHINE_FLAGS) \
	-drive if=pflash,format=raw,unit=0,file=ovmf/ovmf_code-$(ARCH).fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=ovmf/ovmf_vars-$(ARCH).fd \
	-device uefi-vars-x64,jsonfile=uefi_nvram.json \
	-cdrom $(LIVECD) -d guest_errors

.PHONY: genconfig
genconfig: .config
	@python3 utils/kconfiglib/genconfig.py --header-path $(ROOT_DIR)/kernel/include/config.h

.PHONY: menuconfig
menuconfig:
	@python3 utils/kconfiglib/menuconfig.py

.PHONY: format
format:
	@clang-format -i $(shell find . -name "*.c" -o -name "*.h")

.PHONY: clean
clean:
	@$(MAKE) -C boot clean
	@rm -rf $(BUILD_DIR) $(SYSROOT_DIR)

.PHONY: distclean
distclean:
	@rm -rf $(BUILD_DIR) $(SYSROOT_DIR) $(RELEASE_DIR)