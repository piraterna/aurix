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

APP_NAME := doom

DOOM_REPO_URL := https://github.com/piraterna/doomgeneric
DOOM_SRC_DIR := $(CURDIR)/upstream

ifeq ($(notdir $(BUILD_DIR)),$(APP_NAME))
APP_BUILD_DIR := $(BUILD_DIR)
else
APP_BUILD_DIR := $(BUILD_DIR)/$(APP_NAME)
endif

APP_FILE := $(APP_BUILD_DIR)/$(APP_NAME)
DOOM_OUTPUT_FILE := $(APP_BUILD_DIR)/doomgeneric

DOOM_HOST_CC ?= x86_64-aurix-cc
DOOM_TARGET_CC ?= x86_64-aurix-gcc
DOOM_MAKE_FLAGS := O=$(APP_BUILD_DIR)

ifneq (,$(wildcard $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-gcc))
DOOM_CROSS_COMPILE := $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-
else
DOOM_CROSS_COMPILE := $(AURIX_TARGET)-
endif

.PHONY: all clean distclean install rebuild clone

all: $(APP_FILE)

rebuild: clean all

clone: $(DOOM_SRC_DIR)/.git

$(DOOM_SRC_DIR)/.git:
	@printf "  GIT\tclone $(APPNAME)\n"
	@mkdir -p $(@D)
	@git clone $(DOOM_REPO_URL) $(DOOM_SRC_DIR)

$(APP_FILE): clone
	@mkdir -p $(APP_BUILD_DIR)
	@printf "  MAKE\t$(APP_NAME)\n"
	@$(MAKE) -C $(DOOM_SRC_DIR)/doomgeneric -f Makefile.aurix $(DOOM_MAKE_FLAGS) all \
		OBJDIR=$(APP_BUILD_DIR) \
		HOSTCC=$(DOOM_HOST_CC) \
		CROSS_COMPILE=$(DOOM_CROSS_COMPILE) \
		CC=$(DOOM_TARGET_CC) \
		CFLAGS="$(APP_CFLAGS)" \
		LDFLAGS="$(APP_LDFLAGS)"
		if [ -f "$(DOOM_OUTPUT_FILE)" ]; then cp "$(DOOM_OUTPUT_FILE)" "$(APP_FILE)"; exit 0; fi; \
	printf "error: $(APP_NAME) output not found in %s or %s\n" "$(DOOM_OUTPUT_FILE)" "$(DOOM_OUTPUT_FILE_FALLBACK)" >&2; \
	exit 1
ifneq ($(BUILD_TYPE),debug)
	@if [ -n "$(APP_OBJCOPY)" ]; then \
		printf "  STRIP\t$(APP_NAME)\n"; \
		$(APP_OBJCOPY) --strip-unneeded $(APP_FILE); \
	fi
endif

clean:
	@rm -rf $(APP_BUILD_DIR)

distclean: clean
	@rm -rf $(DOOM_SRC_DIR)

install: all
	@printf "  INSTALL\t$(APP_NAME)\n"
	@$(MAKE) -C $(DOOM_SRC_DIR)/doomgeneric -f Makefile.aurix $(DOOM_MAKE_FLAGS) install \
		HOSTCC=$(DOOM_HOST_CC) \
		CROSS_COMPILE=$(DOOM_CROSS_COMPILE) \
		CC=$(DOOM_TARGET_CC) \
		OBJDIR=$(APP_BUILD_DIR) \
		PREFIX=$(APP_INSTALL_DIR)