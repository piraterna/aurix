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

APP_NAME := bash

BASH_VERSION := 5.2
BASH_TARBALL := $(CURDIR)/bash-$(BASH_VERSION).tar.gz
BASH_TARBALL_URL := https://ftp.acc.umu.se/mirror/gnu.org/gnu/bash/bash-$(BASH_VERSION).tar.gz
BASH_SRC_DIR := $(CURDIR)/upstream
BASH_PATCH_DIR := $(CURDIR)/patches
BASH_PATCH_REV := v3
BASH_PATCHES := $(wildcard $(BASH_PATCH_DIR)/*.patch)

ifeq ($(notdir $(BUILD_DIR)),$(APP_NAME))
APP_BUILD_DIR := $(BUILD_DIR)
else
APP_BUILD_DIR := $(BUILD_DIR)/$(APP_NAME)
endif

APP_FILE := $(APP_BUILD_DIR)/$(APP_NAME)
BASH_OUTPUT_FILE := $(APP_BUILD_DIR)/bash

BASH_PATCH_STAMP := $(BASH_SRC_DIR)/.aurix-patched-$(BASH_PATCH_REV)
BASH_UNPACK_STAMP := $(BASH_SRC_DIR)/.aurix-unpacked
BASH_CONFIG_STAMP := $(APP_BUILD_DIR)/.aurix-configured

BASH_HOST_CC ?= cc
BASH_TARGET_CC ?= gcc
BASH_TARGET_AR ?= ar
BASH_TARGET_RANLIB ?= ranlib
BASH_CFLAGS ?= -std=gnu89 -fcommon
BASH_HOST_CFLAGS ?= -std=gnu89 -fcommon
BASH_LDFLAGS ?=
BASH_CONFIGURE_LDFLAGS ?=
BASH_HOST_LDFLAGS ?=
BASH_CONFIGURE_FLAGS ?= --host=$(AURIX_TARGET) --prefix=$(APP_INSTALL_DIR)/.. 

ifneq (,$(wildcard $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-gcc))
BASH_CROSS_COMPILE := $(TOOLCHAIN_DIR)/$(AURIX_TARGET)-
else
BASH_CROSS_COMPILE := $(AURIX_TARGET)-
endif

.PHONY: all clean distclean install rebuild clone patches

all: $(APP_FILE)

rebuild: clean all

clone: $(BASH_UNPACK_STAMP)

patches: $(BASH_PATCH_STAMP)

$(BASH_TARBALL):
	@printf "  GET\tbash tarball\n"
	@mkdir -p $(@D)
	@wget -O "$@" "$(BASH_TARBALL_URL)"

$(BASH_UNPACK_STAMP): $(BASH_TARBALL)
	@printf "  TAR\tunpack bash\n"
	@rm -rf "$(BASH_SRC_DIR)"
	@mkdir -p "$(BASH_SRC_DIR)"
	@tar -xf "$(BASH_TARBALL)" -C "$(BASH_SRC_DIR)" --strip-components=1
	@touch $@

$(BASH_PATCH_STAMP): $(BASH_PATCHES) $(BASH_TARBALL)
	@if [ ! -f "$(BASH_PATCH_STAMP)" ]; then \
		printf "  TAR\treset bash\n"; \
		rm -rf "$(BASH_SRC_DIR)"; \
		mkdir -p "$(BASH_SRC_DIR)"; \
		tar -xf "$(BASH_TARBALL)" -C "$(BASH_SRC_DIR)" --strip-components=1; \
		printf "  PATCH\t$(APP_NAME)\n"; \
		for patch in $(sort $(BASH_PATCHES)); do \
			printf "    -> %s\n" "$$(basename $$patch)"; \
			patch -d "$(BASH_SRC_DIR)" -p1 < "$$patch" || exit $$?; \
		done; \
		rm -f "$(BASH_SRC_DIR)"/.aurix-patched-*; \
		touch "$(BASH_PATCH_STAMP)"; \
	fi

$(BASH_CONFIG_STAMP): $(BASH_PATCH_STAMP)
	@mkdir -p $(APP_BUILD_DIR)
	@if [ -n "$(BUILD_DIR)" ]; then mkdir -p "$(BUILD_DIR)/lib/intl"; fi
	@printf "  CONF\t$(APP_NAME)\n"
	@cd "$(APP_BUILD_DIR)" && \
		"$(BASH_SRC_DIR)/configure" \
			$(BASH_CONFIGURE_FLAGS) \
			CC="$(BASH_CROSS_COMPILE)$(BASH_TARGET_CC)" \
			AR="$(BASH_CROSS_COMPILE)$(BASH_TARGET_AR)" \
			RANLIB="$(BASH_CROSS_COMPILE)$(BASH_TARGET_RANLIB)" \
			CC_FOR_BUILD="$(BASH_HOST_CC)" \
			CFLAGS="$(APP_CFLAGS) $(BASH_CFLAGS)" \
			CFLAGS_FOR_BUILD="$(BASH_HOST_CFLAGS)" \
			LDFLAGS="$(APP_LDFLAGS) $(BASH_CONFIGURE_LDFLAGS)" \
			LDFLAGS_FOR_BUILD="$(BASH_HOST_LDFLAGS)" \
			HOSTCC="$(BASH_HOST_CC)"
	@touch $@

$(APP_FILE): $(BASH_CONFIG_STAMP)
	@mkdir -p $(APP_BUILD_DIR)
	@printf "  MAKE\t$(APP_NAME)\n"
	@if [ -n "$(BUILD_DIR)" ]; then mkdir -p "$(BUILD_DIR)/lib/intl"; fi
	@$(MAKE) -C $(APP_BUILD_DIR) BUILD_DIR=$(APP_BUILD_DIR) \
		CC="$(BASH_CROSS_COMPILE)$(BASH_TARGET_CC)" \
		AR="$(BASH_CROSS_COMPILE)$(BASH_TARGET_AR)" \
		RANLIB="$(BASH_CROSS_COMPILE)$(BASH_TARGET_RANLIB)" \
		CC_FOR_BUILD="$(BASH_HOST_CC)" \
		CFLAGS_FOR_BUILD="$(BASH_HOST_CFLAGS)" \
		LDFLAGS_FOR_BUILD="$(BASH_HOST_LDFLAGS)" \
		HOSTLDFLAGS="$(BASH_HOST_LDFLAGS)" \
		LDFLAGS="$(APP_LDFLAGS) $(BASH_LDFLAGS)"
	@if [ ! -f "$(BASH_OUTPUT_FILE)" ]; then \
		printf "error: bash output not found at %s\n" "$(BASH_OUTPUT_FILE)" >&2; \
		exit 1; \
	fi
ifneq ($(BUILD_TYPE),debug)
	@if [ -n "$(APP_OBJCOPY)" ]; then \
		printf "  STRIP\t$(APP_NAME)\n"; \
		$(APP_OBJCOPY) --strip-unneeded $(APP_FILE); \
	fi
endif

clean:
	@rm -rf $(APP_BUILD_DIR)
	@rm -f $(BASH_CONFIG_STAMP)

distclean: clean
	@rm -f $(BASH_PATCH_STAMP)
	@rm -f $(BASH_UNPACK_STAMP)
	@rm -f $(BASH_TARBALL)
	@rm -rf $(BASH_SRC_DIR)

install: all
	@printf "  INSTALL\t$(APP_NAME)\n"
	@mkdir -p $(APP_INSTALL_DIR)
	@cp $(APP_FILE) $(APP_INSTALL_DIR)
	@ln -sf $(APP_NAME) $(APP_INSTALL_DIR)/sh
