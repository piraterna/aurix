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
all: boot kernel
	@:

.PHONY: boot
boot:
	@printf ">>> Building bootloader...\n"
	@$(MAKE) -C boot all

.PHONY: kernel
kernel:
	@printf ">>> Building kernel...\n"
	@$(MAKE) -C kernel

.PHONY: install
install: boot kernel
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

ovmf:
	@printf ">>> Downloading OVMF images...\n"
	@utils/download-ovmf.sh

nvram:
	@printf ">>> Generating NVRAM image...\n"
	@if [ ! -f nvram.json ]; then \
		echo "Error: nvram.json not found in $(ROOT_DIR)"; \
		exit 1; \
	fi
	@if ! command -v jq >/dev/null 2>&1; then \
		echo "Error: 'jq' is required but not installed"; \
		exit 1; \
	fi
	@INPUT_FD=$(ROOT_DIR)/ovmf/ovmf-$(ARCH)-vars.fd; \
	OUTPUT_FD=$(ROOT_DIR)/nvram.fd; \
	TEMP_FD=$(ROOT_DIR)/nvram_temp.fd; \
	if [ ! -f "$$INPUT_FD" ]; then \
		echo "Error: Input file $$INPUT_FD does not exist"; \
		exit 1; \
	fi; \
	if ! jq -e '.variables' nvram.json >/dev/null; then \
		echo "Error: nvram.json is missing 'variables' array or is invalid JSON"; \
		rm -f $$TEMP_FD; \
		exit 1; \
	fi; \
	VARS=$$(jq -c '.variables[]' nvram.json); \
	INDEX=0; \
	cp $$INPUT_FD $$OUTPUT_FD; \
	for VAR in $$VARS; do \
		if ! echo "$$VAR" | jq -e '.vendor_uuid and .name and .data' >/dev/null; then \
			echo "Error: Variable at index $$INDEX is missing required fields (vendor_uuid, name, data)"; \
			rm -f $$TEMP_FD $$OUTPUT_FD; \
			exit 1; \
		fi; \
		VENDOR_UUID=$$(echo "$$VAR" | jq -r '.vendor_uuid'); \
		NAME=$$(echo "$$VAR" | jq -r '.name'); \
		DATA=$$(echo "$$VAR" | jq -r '.data'); \
		if [ $$INDEX -eq 0 ]; then \
			./utils/nvram.sh $$INPUT_FD $$OUTPUT_FD "$$VENDOR_UUID" "$$NAME" "$$DATA"; \
		else \
			cp $$OUTPUT_FD $$TEMP_FD; \
			./utils/nvram.sh $$TEMP_FD $$OUTPUT_FD "$$VENDOR_UUID" "$$NAME" "$$DATA"; \
		fi; \
		if [ $$? -ne 0 ]; then \
			echo "Error: Failed to process variable at index $$INDEX"; \
			rm -f $$TEMP_FD $$OUTPUT_FD; \
			exit 1; \
		fi; \
		INDEX=$$((INDEX + 1)); \
	done; \
	rm -f $$TEMP_FD; \
	if [ $$INDEX -eq 0 ]; then \
		echo "Warning: No variables defined in nvram.json; copied $$INPUT_FD to $$OUTPUT_FD"; \
	else \
		echo "Successfully generated $$OUTPUT_FD with $$INDEX variables"; \
	fi

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

.PHONY: run-uefi
run-uefi: livecd ovmf nvram
	@printf ">>> Running QEMU (UEFI)...\n"
	@qemu-system-$(ARCH) $(QEMU_FLAGS) $(QEMU_MACHINE_FLAGS) \
	-drive if=pflash,format=raw,unit=0,file=ovmf/ovmf-$(ARCH).fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=nvram.fd \
	-cdrom $(LIVECD) -d guest_errors	

.PHONY: format
format:
	@clang-format -i $(shell find . -name "*.c" -o -name "*.h")

.PHONY: clean
clean:
	@$(MAKE) -C boot clean
	@rm -rf $(BUILD_DIR) $(SYSROOT_DIR)

.PHONY: distclean
distclean:
	@rm -rf $(BUILD_DIR) $(SYSROOT_DIR) $(RELEASE_DIR) ovmf/