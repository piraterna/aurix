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

UEFI_DRIVER := $(BUILD_DIR)/boot/uefi/drivers/intel-hda.efi

INCLUDE_DIRS += $(BOOT_ROOT)/drivers/intel-hda \
				$(BOOT_ROOT)/drivers/intel-hda/include \
				$(BOOT_ROOT)/platform/uefi/libefi

UEFI_CC := clang
UEFI_LD := clang

UEFI_ASFLAGS := $(ASFLAGS) \
				-DAXBOOT_UEFI=1 \
				$(foreach d, $(INCLUDE_DIRS), -I$d)

UEFI_CFLAGS := $(CFLAGS) \
				-DAXBOOT_UEFI=1 \
				$(foreach d, $(INCLUDE_DIRS), -I$d) \
				-target $(ARCH)-unknown-windows \
				-fshort-wchar \
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
				-Wl,-subsystem:efi_boot_service_driver \
				-Wl,-entry:InitHda

DRIVER_CFILES := $(shell find $(BOOT_ROOT)/drivers/intel-hda -name '*.c')

UEFI_OBJ := $(DRIVER_CFILES:$(BOOT_ROOT)/drivers/intel-hda/%.c=$(BUILD_DIR)/boot/uefi/drivers/intel-hda/%.c.o)

.PHONY: all
all: $(UEFI_DRIVER)

.PHONY: install
install: $(UEFI_DRIVER)
	@mkdir -p $(SYSROOT_DIR)/AxBoot/drivers
	@printf "  INSTALL\t/AxBoot/drivers/$(shell basename $(UEFI_DRIVER))\n"
	@cp $(UEFI_DRIVER) $(SYSROOT_DIR)/AxBoot/drivers/

$(UEFI_DRIVER): $(UEFI_OBJ)
	@mkdir -p $(@D)
	@printf "  LD\t$(notdir $@)\n"
	@$(UEFI_LD) $(UEFI_LDFLAGS) $^ -o $@

-include $(wildcard $(BUILD_DIR)/boot/*.d)

$(BUILD_DIR)/boot/uefi/drivers/intel-hda/%.c.o: $(BOOT_ROOT)/drivers/intel-hda/%.c
	@mkdir -p $(@D)
	@printf "  CC\t$(subst $(ROOT_DIR)/,,$<)\n"
	@$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@
