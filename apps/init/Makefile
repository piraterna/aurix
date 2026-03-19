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

APP_NAME := init

BUILD_DIR := $(BUILD_DIR)/$(APP_NAME)
APP_FILE := $(BUILD_DIR)/$(APP_NAME)

APP_INCLUDE_DIRS := include
APP_DEFINES := __$(ARCH)__
APP_DEFINES += APP_NAME=$(APP_NAME)

APP_CFLAGS += $(foreach d,$(APP_INCLUDE_DIRS),-I$d) \
              $(foreach d,$(APP_DEFINES),-D$d)

APP_LDFLAGS +=

APP_CFILES := $(shell find . -name '*.c')
APP_OBJ := $(APP_CFILES:%.c=$(BUILD_DIR)/%.c.o)
APP_DEP := $(APP_OBJ:.o=.d)

.PHONY: all clean install

all: $(APP_FILE)

install: all
	@mkdir -p $(APP_INSTALL_DIR)
	@cp $(APP_FILE) $(APP_INSTALL_DIR)

clean:
	@rm -rf $(BUILD_DIR)

-include $(APP_DEP)

$(APP_FILE): $(APP_OBJ)
	@mkdir -p $(@D)
	@printf "  LD\t$(notdir $@)\n"
	@$(APP_CC) $(APP_OBJ) $(APP_LDFLAGS) -o $@
ifneq ($(BUILD_TYPE),debug)
	@printf "  STRIP\t$(notdir $@)\n"
	@$(APP_OBJCOPY) --strip-unneeded $@
endif

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	@printf "  CC\t$<\n"
	@$(APP_CC) $(APP_CFLAGS) -MMD -MP -c $< -o $@
