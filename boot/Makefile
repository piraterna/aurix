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

export INCLUDE_DIRS := $(BOOT_ROOT)/include

export DEFINES += __$(ARCH)__ \
				_AXBOOT

export BUILD_DIR ?= build
export SYSROOT_DIR ?= sysroot

export ASFLAGS := $(foreach d, $(DEFINES), -D$d)
export CFLAGS := $(foreach d, $(DEFINES), -D$d) -Wall -Wextra -ffreestanding -fno-stack-protector -fno-stack-check -MMD -MP
export LDFLAGS := -nostdlib

export BOOT_ROOT := $(ROOT_DIR)/boot

ifeq ($(BUILD_TYPE),debug)
CFLAGS += -O0 -g3
else
CFLAGS += -O2
endif

.PHONY: all
all: sounds
ifneq (,$(filter $(ARCH),i686 x86_64))
	@$(MAKE) -C platform/pc-bios all
else
	@$(MAKE) -C platform/$(PLATFORM) all
endif
ifneq (,$(filter $(ARCH),i686 x86_64 arm32 aarch64))
ifeq ($(NOUEFI),n)
	@$(MAKE) -C platform/uefi all
#	@$(MAKE) -C drivers all
endif
endif

.PHONY: sounds
sounds:
	@mkdir -p $(BUILD_DIR)/boot/sounds
	@mkdir -p $(BOOT_ROOT)/include/sounds
	@for f in $(BOOT_ROOT)/sound/*.mp3; do \
		file=$$(basename $$f ".$${f##*.}") ; \
		printf "  GEN\t$$file.h\n" ; \
    	ffmpeg -i "$$f" -acodec pcm_s16le -f s16le -ac 2 "$(BUILD_DIR)/boot/sounds/$$file.raw" -y 2>/dev/null ; \
		python3 $(ROOT_DIR)/utils/bin_to_header.py "$(BUILD_DIR)/boot/sounds/$$file.raw" "$(BOOT_ROOT)/include/sounds/$$file.h" $$file ; \
	done

.PHONY: install
install:
	@$(MAKE) -C platform/$(PLATFORM) install
#	@$(MAKE) -C drivers install
	@mkdir -p $(SYSROOT_DIR)/AxBoot
	@cp -r base/* $(SYSROOT_DIR)/AxBoot

.PHONY: clean
clean:
	@rm -rf $(BOOT_ROOT)/include/sounds
