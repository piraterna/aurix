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

export BUILD_DIR := $(BUILD_DIR)/src
export APP_INSTALL_DIR := $(INITRD_DIR)/bin

export TOOLCHAIN_DIR ?= $(ROOT_DIR)/libc/toolchain/usr/bin
export AURIX_TARGET ?= $(ARCH)-aurix

ifneq (,$(wildcard $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-gcc))
export APP_AS := $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-gcc
export APP_CC := $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-gcc
export APP_LD := $(APP_CC)
export APP_AR := $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-ar
export APP_OBJCOPY := $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-objcopy
else
$(warning Aurix toolchain not found. Run ./libc/build.sh to install it.)
export APP_AS := $(AURIX_TARGET)-gcc
export APP_CC := $(AURIX_TARGET)-gcc
export APP_LD := $(APP_CC)
export APP_AR := $(AURIX_TARGET)-ar
export APP_OBJCOPY := $(AURIX_TARGET)-objcopy
endif

export APP_CFLAGS := -Wall -Wextra -I$(abspath include)
export APP_LDFLAGS :=

export APP_DEFINES := __$(ARCH)__

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
