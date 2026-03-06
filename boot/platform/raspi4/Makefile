###################################################################################
## Module Name:  Makefile                                                        ##
## Project:      AurixOS                                                         ##
##                                                                               ##
## Copyright (c) 2024-2026 Jozef Nagy                                            ##
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

BOOTFILE := $(BUILD_DIR)/boot/raspi4/kernel8.img

INCLUDE_DIRS += $(BOOT_ROOT)/include \
				$(BOOT_ROOT)/include/arch/$(ARCH) \
				$(BOOT_ROOT)/include/platform/$(PLATFORM)

BOOT_AS := $(ARCH)-elf-gcc
BOOT_CC := $(ARCH)-elf-gcc
BOOT_LD := $(ARCH)-elf-ld
BOOT_OBJCOPY := $(ARCH)-elf-objcopy

BOOT_ASFLAGS := $(ASFLAGS) \
				$(foreach d, $(INCLUDE_DIRS), -I$d)

BOOT_CFLAGS := $(CFLAGS) \
				$(foreach d, $(INCLUDE_DIRS), -I$d) \
				-mstrict-align \
				-nostartfiles

BOOT_LDFLAGS := $(LDFLAGS) \
				-T$(BOOT_ROOT)/arch/$(ARCH)/linker.ld

COMMON_CFILES := $(shell find $(BOOT_ROOT)/common -name '*.c')
COMMON_ARCH_CFILES := $(shell find $(BOOT_ROOT)/arch/$(ARCH)/common -name '*.c')
COMMON_ARCH_ASFILES := $(shell find $(BOOT_ROOT)/arch/$(ARCH)/common -name '*.S')
PLATFORM_CFILES := $(shell find $(BOOT_ROOT)/platform/$(PLATFORM) -name '*.c')
PLATFORM_ASFILES := $(shell find $(BOOT_ROOT)/platform/$(PLATFORM) -name '*.S')

BOOT_OBJ := $(COMMON_CFILES:$(BOOT_ROOT)/common/%.c=$(BUILD_DIR)/boot/$(PLATFORM)/common/%.c.o) \
			$(COMMON_ARCH_CFILES:$(BOOT_ROOT)/arch/$(ARCH)/common/%.c=$(BUILD_DIR)/boot/$(PLATFORM)/arch/%.c.o) \
			$(COMMON_ARCH_ASFILES:$(BOOT_ROOT)/arch/$(ARCH)/common/%.S=$(BUILD_DIR)/boot/$(PLATFORM)/arch/%.S.o) \
			$(PLATFORM_CFILES:$(BOOT_ROOT)/platform/$(PLATFORM)/%.c=$(BUILD_DIR)/boot/$(PLATFORM)/%.c.o) \
			$(PLATFORM_ASFILES:$(BOOT_ROOT)/platform/$(PLATFORM)/%.S=$(BUILD_DIR)/boot/$(PLATFORM)/%.S.o)

.PHONY: all
all: $(BOOTFILE)
	@:

.PHONY: install
install: all
	@mkdir -p $(SYSROOT_DIR)
	@printf "  INSTALL\t/kernel8.img\n"
	@cp $(BOOTFILE) $(SYSROOT_DIR)/kernel8.img

$(BOOTFILE): $(BOOT_OBJ)
	@mkdir -p $(@D)
	@printf "  LD\taxboot.elf\n"
	@$(BOOT_LD) $(BOOT_LDFLAGS) $^ -o $(BUILD_DIR)/boot/raspi4/axboot.elf
	@printf "  OCOPY\t$(notdir $@)\n"
	@$(BOOT_OBJCOPY) -O binary $(BUILD_DIR)/boot/raspi4/axboot.elf $@

-include $(wildcard $(BUILD_DIR)/boot/*.d)

$(BUILD_DIR)/boot/raspi4/%.c.o: $(BOOT_ROOT)/platform/$(PLATFORM)/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(BOOT_CC) $(BOOT_CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/raspi4/%.S.o: $(BOOT_ROOT)/platform/$(PLATFORM)/%.S
	@mkdir -p $(@D)
	@printf "  AS\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(BOOT_AS) $(BOOT_ASFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/raspi4/common/%.c.o: $(BOOT_ROOT)/common/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(BOOT_CC) $(BOOT_CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/raspi4/arch/%.c.o: $(BOOT_ROOT)/arch/$(ARCH)/common/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(BOOT_CC) $(BOOT_CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/raspi4/arch/%.S.o: $(BOOT_ROOT)/arch/$(ARCH)/common/%.S
	@mkdir -p $(@D)
	@printf "  AS\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(BOOT_AS) $(BOOT_ASFLAGS) -c $< -o $@