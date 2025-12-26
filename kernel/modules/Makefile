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

MAKE_TARGETS := all install clean
MOD_DIRS = $(foreach v,$(wildcard */.),$(if $(findstring .nocomp,$v),,$v))

export BUILD_DIR := $(BUILD_DIR)/kernel/mod
export MOD_INSTALL_DIR := $(SYSROOT_DIR)/System/support

export MOD_CFLAGS := -Wall -Wextra -ffreestanding -fno-pic -fno-pie -I$(abspath ../include/aurix)
export MOD_LDFLAGS := -nostdlib -static
export MOD_DEFINES := __$(ARCH)__

export MOD_AS := $(ARCH)-elf-gcc
export MOD_CC := $(ARCH)-elf-gcc
export MOD_LD := $(ARCH)-elf-ld
export MOD_OBJCOPY := $(ARCH)-elf-objcopy

ifeq ($(BUILD_TYPE),debug)
export MOD_CFLAGS += -O0 -g3 -D_DEBUG
else
export MOD_CFLAGS += -O2
endif

$(MAKE_TARGETS): $(MOD_DIRS)

$(MOD_DIRS):
ifeq ($(MAKECMDGOALS),)
	@printf ">>> Building module '$(patsubst %/,%,$(dir $(@)))'...\n"
endif
	@$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(MAKE_TARGETS) $(MOD_DIRS)