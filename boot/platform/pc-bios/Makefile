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

ARCH_COMMON := x86
INCLUDE_DIRS += include/arch/$(ARCH)

BOOT_AS := nasm
BOOT_CC := $(ARCH)-elf-gcc
BOOT_LD := $(ARCH)-elf-ld

BOOT_ASFLAGS := $(ASFLAGS) \
				$(foreach d, $(INCLUDE_DIRS), -I$d)
BOOT_CFLAGS := $(CFLAGS) \
				-mno-red-zone \
				-mno-stack-arg-probe

BOOT_LDFLAGS := $(LDFLAGS)

BOOT_ASFILES := $(shell find $(BOOT_ROOT)/arch/$(ARCH) -type d -wholename arch/$(ARCH)/uefi -prune -name '*.S')
BOOT_CFILES := $(shell find $(BOOT_ROOT)/arch/$(ARCH) -type d -wholename arch/$(ARCH)/uefi -prune -name '*.c')

BOOT_OBJ := $(BOOT_CFILES:%.c=$(BUILD_DIR)/boot/boot/%.c.o) \
			$(BOOT_ASFILES:%.S=$(BUILD_DIR)/boot/boot/%.S.o) \
			$(COMMON_CFILES:common/%.c=$(BUILD_DIR)/boot/boot/common/%.c.o)

# stage 1 bootloader
STAGE1_HDD := $(BUILD_DIR)/boot/pc-bios/stage1-hdd.bin
STAGE1_CD := $(BUILD_DIR)/boot/pc-bios/stage1-cd.bin
STAGE1 := $(STAGE1_HDD) $(STAGE1_CD)

.PHONY: all
all: $(STAGE1)

.PHONY: install
install:
	@:

# stage 1
$(STAGE1_HDD): $(BOOT_ROOT)/arch/$(ARCH)/stage1/boot-hdd.asm
	@mkdir -p $(@D)
	@printf "  AS\tboot/arch/$(ARCH)/stage1/stage1-hdd.bin\n"
	@nasm -fbin -I$(BOOT_ROOT)/arch/$(ARCH)/stage1 $(ASFLAGS) $< -o $@

$(STAGE1_CD): $(BOOT_ROOT)/arch/$(ARCH)/stage1/boot-cd.asm
	@mkdir -p $(@D)
	@printf "  AS\tboot/arch/$(ARCH)/stage1/stage1-cd.bin\n"
	@nasm -fbin -I$(BOOT_ROOT)/arch/$(ARCH)/stage1 $(ASFLAGS) $< -o $@
