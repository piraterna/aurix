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

ifeq ($(ARCH),x86_64)
UEFISUF := X64
else ifeq ($(ARCH),i686)
UEFISUF := IA32
else ifeq ($(ARCH),arm)
UEFISUF := ARM
else ifeq ($(ARCH),aarch64)
UEFISUF := AA64
else ifeq ($(ARCH),riscv64)
UEFISUF := RISCV64
else
$(error Architecture not supported!)
endif

UEFI_BOOTFILE := $(BUILD_DIR)/boot/uefi/BOOT$(UEFISUF).EFI

INCLUDE_DIRS += $(BOOT_ROOT)/include \
				$(BOOT_ROOT)/include/arch/$(ARCH) \
				$(BOOT_ROOT)/include/arch/$(ARCH)/uefi \
				$(BOOT_ROOT)/include/platform/$(PLATFORM) \
				$(BOOT_ROOT)/platform/uefi/libefi \
				$(BOOT_ROOT)/platform/uefi/drivers/intel-hda

UEFI_AS := $(ARCH)-w64-mingw32-as
UEFI_CC := clang
UEFI_LD := clang

UEFI_ASFLAGS := $(ASFLAGS) \
				-DAXBOOT_UEFI=1 \
				$(foreach d, $(INCLUDE_DIRS), -I$d)

UEFI_CFLAGS := $(CFLAGS) \
				-DAXBOOT_UEFI=1 \
				$(foreach d, $(INCLUDE_DIRS), -I$d) \
				-target $(ARCH)-unknown-windows \
				-std=c11 \
				-flto \
				-fshort-wchar \
				-msoft-float \
				-mno-red-zone \
				-mno-stack-arg-probe

ifneq (,$(filter $(ARCH),i686 x86_64))
UEFI_CFLAGS += -mno-80387 \
				-mno-mmx \
				-mno-sse \
				-mno-sse2
endif

UEFI_LDFLAGS := $(LDFLAGS) \
				-target $(ARCH)-unknown-windows \
				-fuse-ld=lld-link \
				-Wl,-subsystem:efi_application \
				-Wl,-entry:uefi_entry

COMMON_CFILES := $(shell find $(BOOT_ROOT)/common -name '*.c')
COMMON_ARCH_CFILES := $(shell find $(BOOT_ROOT)/arch/$(ARCH)/common -name '*.c')
COMMON_ARCH_ASFILES := $(shell find $(BOOT_ROOT)/arch/$(ARCH)/common -name '*.S')
UEFI_CFILES := $(shell find $(BOOT_ROOT)/platform/uefi -name '*.c') $(shell find $(BOOT_ROOT)/arch/$(ARCH)/uefi -name '*.c')
UEFI_ASFILES := $(shell find $(BOOT_ROOT)/platform/uefi -name '*.S') $(shell find $(BOOT_ROOT)/arch/$(ARCH)/uefi -name '*.S')

UEFI_OBJ := $(UEFI_CFILES:$(BOOT_ROOT)/platform/uefi/%.c=$(BUILD_DIR)/boot/uefi/%.c.o) \
			$(UEFI_ASFILES:$(BOOT_ROOT)/platform/uefi/%.S=$(BUILD_DIR)/boot/uefi/%.S.o) \
			$(COMMON_CFILES:$(BOOT_ROOT)/common/%.c=$(BUILD_DIR)/boot/uefi/common/%.c.o) \
			$(COMMON_ARCH_CFILES:$(BOOT_ROOT)/arch/$(ARCH)/common/%.c=$(BUILD_DIR)/boot/uefi/arch/%.c.o) \
			$(COMMON_ARCH_ASFILES:$(BOOT_ROOT)/arch/$(ARCH)/common/%.asm=$(BUILD_DIR)/boot/uefi/arch/%.asm.o)

.PHONY: all
all: $(UEFI_BOOTFILE)

.PHONY: install
install:
	@mkdir -p $(SYSROOT_DIR)/EFI/BOOT
	@printf "  INSTALL\t/EFI/BOOT/BOOT$(UEFISUF).EFI\n"
	@cp $(UEFI_BOOTFILE) $(SYSROOT_DIR)/EFI/BOOT/

$(UEFI_BOOTFILE): $(UEFI_OBJ)
	@mkdir -p $(@D)
	@printf "  LD\t$(notdir $@)\n"
	@$(UEFI_LD) $(UEFI_LDFLAGS) $^ -o $@

-include $(wildcard $(BUILD_DIR)/boot/*.d)

$(BUILD_DIR)/boot/uefi/%.c.o: $(BOOT_ROOT)/platform/uefi/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/uefi/common/%.c.o: $(BOOT_ROOT)/common/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/uefi/arch/%.c.o: $(BOOT_ROOT)/arch/$(ARCH)/common/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/uefi/arch/%.asm.o: $(BOOT_ROOT)/arch/$(ARCH)/common/%.asm
	@mkdir -p $(@D)
	@printf "  AS\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(UEFI_AS) $(UEFI_ASFLAGS) -c $< -o $@