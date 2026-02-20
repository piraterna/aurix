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

MOD_NAME := dummy

BUILD_DIR := $(BUILD_DIR)/$(MOD_NAME)

MOD_FILE := $(BUILD_DIR)/$(MOD_NAME).sys

MOD_INCLUDE_DIRS := include
MOD_DEFINES := __$(ARCH)__

MOD_CFLAGS += $(foreach d, $(MOD_INCLUDE_DIRS), -I$d) \
				$(foreach d, $(MOD_DEFINES), -D$d)
MOD_LDFLAGS +=

MOD_CFILES := $(shell find . -name '*.c') ../common/axapi.c
MOD_OBJ := $(MOD_CFILES:%.c=$(BUILD_DIR)/%.c.o)

.PHONY: all
all: $(MOD_FILE)

.PHONY: install
install: all
	@mkdir -p $(MOD_INSTALL_DIR)
	@cp $(MOD_FILE) $(MOD_INSTALL_DIR)

.PHONY: clean
clean:
	@rm -rf $(MOD_FILE)

-include $(wildcard $(BUILD_DIR)/mods/$(MOD_NAME)/*.d)

$(MOD_FILE): $(MOD_OBJ)
	@mkdir -p $(@D)
	@printf "  LD\t$(notdir $@)\n"
	@$(MOD_LD) $(MOD_LDFLAGS) $^ -o $@
ifneq ($(BUILD_TYPE),debug)
	@printf "  OBJCOPY m_$(MOD_NAME).sym\n"
	@$(MOD_OBJCOPY) --only-keep-debug $@ $(BUILD_DIR)/m_$(MOD_NAME).sym
	@$(MOD_OBJCOPY) --strip-debug --strip-unneeded $@
endif

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	@printf "  CC\t$<\n"
	@$(MOD_CC) $(MOD_CFLAGS) -c $< -o $@
