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

APP_NAME := toybox

TOYBOX_REPO_URL := https://github.com/landley/toybox
TOYBOX_SRC_DIR := $(CURDIR)/upstream
TOYBOX_PATCH_DIR := $(CURDIR)/patches
TOYBOX_MINICONFIG := $(CURDIR)/aurix_miniconfig
TOYBOX_PATCH_STAMP := $(TOYBOX_SRC_DIR)/.aurix-patched-v4
TOYBOX_CONFIG_STAMP := $(TOYBOX_SRC_DIR)/.aurix-configured
TOYBOX_PATCHES := $(wildcard $(TOYBOX_PATCH_DIR)/*.patch)

ifeq ($(notdir $(BUILD_DIR)),$(APP_NAME))
APP_BUILD_DIR := $(BUILD_DIR)
else
APP_BUILD_DIR := $(BUILD_DIR)/$(APP_NAME)
endif

APP_FILE := $(APP_BUILD_DIR)/$(APP_NAME)
TOYBOX_OUTPUT_FILE := $(TOYBOX_SRC_DIR)/generated/unstripped/toybox

TOYBOX_HOST_CC ?= cc
TOYBOX_TARGET_CC ?= gcc
TOYBOX_CONFIG_TARGET ?= allnoconfig
TOYBOX_MAKE_FLAGS := O=$(APP_BUILD_DIR)

ifneq (,$(wildcard $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-gcc))
TOYBOX_CROSS_COMPILE := $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-
else
TOYBOX_CROSS_COMPILE := $(AURIX_TARGET)-
endif

.PHONY: all clean install rebuild clone patches

all: $(APP_FILE)

rebuild: clean all

clone: $(TOYBOX_SRC_DIR)/.git

patches: $(TOYBOX_PATCH_STAMP)

$(TOYBOX_SRC_DIR)/.git:
	@printf "  GIT\tclone toybox\n"
	@mkdir -p $(@D)
	@git clone $(TOYBOX_REPO_URL) $(TOYBOX_SRC_DIR)

$(TOYBOX_PATCH_STAMP): $(TOYBOX_PATCHES) | $(TOYBOX_SRC_DIR)/.git
	@old_stamps="$$(ls -1 $(TOYBOX_SRC_DIR)/.aurix-patched-* 2>/dev/null || true)"; \
	if [ -n "$$old_stamps" ] && [ ! -f "$(TOYBOX_PATCH_STAMP)" ]; then \
		printf "  GIT\treclone toybox (patch rev)\n"; \
		rm -rf "$(TOYBOX_SRC_DIR)"; \
		git clone $(TOYBOX_REPO_URL) $(TOYBOX_SRC_DIR); \
	fi
	@if [ ! -f "$(TOYBOX_PATCH_STAMP)" ] && [ -n "$$(git -C $(TOYBOX_SRC_DIR) status --porcelain --untracked-files=no 2>/dev/null || true)" ]; then \
		printf "  GIT\treclone toybox (dirty tree)\n"; \
		rm -rf "$(TOYBOX_SRC_DIR)"; \
		git clone $(TOYBOX_REPO_URL) $(TOYBOX_SRC_DIR); \
	fi
	@printf "  PATCH\t$(APP_NAME)\n"
	@if [ -n "$(strip $(TOYBOX_PATCHES))" ]; then \
		for patch in $(sort $(TOYBOX_PATCHES)); do \
			printf "    -> %s\n" "$$(basename $$patch)"; \
			if patch -R -s -f --dry-run -d $(TOYBOX_SRC_DIR) -p1 < "$$patch" >/dev/null 2>&1; then \
				printf "       already applied\n"; \
			else \
				patch -d $(TOYBOX_SRC_DIR) -p1 < "$$patch" || exit $$?; \
			fi; \
		done; \
	fi
	@touch $@

$(TOYBOX_CONFIG_STAMP): $(TOYBOX_PATCH_STAMP) $(TOYBOX_MINICONFIG)
	@if [ -f "$(APP_BUILD_DIR)" ]; then rm -f "$(APP_BUILD_DIR)"; fi
	@mkdir -p $(APP_BUILD_DIR)
	@printf "  CONF\t$(APP_NAME)\n"
	@$(MAKE) -C $(TOYBOX_SRC_DIR) $(TOYBOX_MAKE_FLAGS) \
		KCONFIG_ALLCONFIG=$(TOYBOX_MINICONFIG) \
		$(TOYBOX_CONFIG_TARGET) HOSTCC=$(TOYBOX_HOST_CC)
	@touch $@

$(APP_FILE): $(TOYBOX_CONFIG_STAMP)
	@if [ -f "$(APP_BUILD_DIR)" ]; then rm -f "$(APP_BUILD_DIR)"; fi
	@mkdir -p $(APP_BUILD_DIR)
	@printf "  MAKE\t$(APP_NAME)\n"
	@$(MAKE) -C $(TOYBOX_SRC_DIR) $(TOYBOX_MAKE_FLAGS) toybox \
		HOSTCC=$(TOYBOX_HOST_CC) \
		CROSS_COMPILE=$(TOYBOX_CROSS_COMPILE) \
		CC=$(TOYBOX_TARGET_CC) \
		CFLAGS="$(APP_CFLAGS)" \
		LDFLAGS="$(APP_LDFLAGS)"
	@cp $(TOYBOX_OUTPUT_FILE) $(APP_FILE)
ifneq ($(BUILD_TYPE),debug)
	@if [ -n "$(APP_OBJCOPY)" ]; then \
		printf "  STRIP\t$(APP_NAME)\n"; \
		$(APP_OBJCOPY) --strip-unneeded $(APP_FILE); \
	fi
endif

clean:
	@rm -rf $(APP_BUILD_DIR)

install: all
	@mkdir -p $(APP_INSTALL_DIR)
	@cp $(APP_FILE) $(APP_INSTALL_DIR)
