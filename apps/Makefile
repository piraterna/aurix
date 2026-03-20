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

MAKE_TARGETS := all install clean rebuild

APP_DIRS := $(wildcard */)
APP_DIRS := $(filter-out include/,$(APP_DIRS))
APP_DIRS := $(filter-out %.nocomp/,$(APP_DIRS))

export BUILD_DIR := $(BUILD_DIR)/apps
export APP_INSTALL_DIR := $(INITRD_DIR)/bin

export APP_CFLAGS := -Wall -Wextra -ffreestanding \
                     -fno-stack-protector -fno-pie -I$(abspath include)
export APP_LDFLAGS := -nostdlib -nostartfiles -nodefaultlibs -static -Wl,-e,_start

export APP_DEFINES := __$(ARCH)__

ifeq ($(USE_HOST_TOOLCHAIN),y)
export APP_AS := gcc
export APP_CC := gcc
export APP_LD := $(APP_CC)
export APP_AR := ar
export APP_OBJCOPY := objcopy
else
export APP_AS := $(ARCH)-elf-gcc
export APP_CC := $(ARCH)-elf-gcc
export APP_LD := $(APP_CC)
export APP_AR := $(ARCH)-elf-ar
export APP_OBJCOPY := $(ARCH)-elf-objcopy
endif

ifeq ($(BUILD_TYPE),debug)
export APP_CFLAGS += -O0 -g3 -D_DEBUG
else
export APP_CFLAGS += -O2
endif

.PHONY: $(MAKE_TARGETS)

$(MAKE_TARGETS):
	@for dir in $(APP_DIRS); do \
		printf ">>> Building app '%s'...\n" "$${dir%/}"; \
		$(MAKE) -C "$$dir" $(MAKECMDGOALS) \
			APP_CFLAGS="$(APP_CFLAGS)" \
			APP_LDFLAGS="$(APP_LDFLAGS)" \
			APP_CC="$(APP_CC)" \
			APP_OBJCOPY="$(APP_OBJCOPY)" \
			APP_INSTALL_DIR="$(APP_INSTALL_DIR)" || exit $$?; \
	done