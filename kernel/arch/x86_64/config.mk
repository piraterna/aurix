###################################################################################
## Module Name:  config.mk                                                       ##
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

KERNEL_CFLAGS += -m64 \
				-march=x86-64 \
				-mabi=sysv \
				-mcmodel=kernel \
				-mno-red-zone \
				-mno-80387 \
				-mno-mmx \
				-mno-sse \
				-mno-sse2

KERNEL_LDFLAGS += -nostdlib \
				--no-dynamic-linker \
				-z max-page-size=0x1000 \
				-z text \
				-melf_x86_64
