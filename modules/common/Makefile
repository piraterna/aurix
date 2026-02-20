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

MOD_NAME := common

BUILD_DIR := $(BUILD_DIR)/$(MOD_NAME)

COMMON_LIB := $(BUILD_DIR)/libaxapi.a

COMMON_CFILES := axapi.c
COMMON_OBJ := $(COMMON_CFILES:%.c=$(BUILD_DIR)/%.c.o)

.PHONY: all
all: $(COMMON_LIB)

.PHONY: install
install: all
	@true

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

$(COMMON_LIB): $(COMMON_OBJ)
	@mkdir -p $(@D)
	@printf "  AR\t$(notdir $@)\n"
	@rm -f $@
	@$(MOD_AR) rcs $@ $^

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	@printf "  CC\t$<\n"
	@$(MOD_CC) $(MOD_CFLAGS) -c $< -o $@
