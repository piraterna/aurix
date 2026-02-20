/*********************************************************************************/
/* Module Name:  linker.ld                                                       */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

/* This is just a copied linker from the bootloader. */
/* Not cool. But it's good enough... for now. */

SECTIONS
{
    . = 0x80000;
    .text : {
		*(.text .text.*)
		*(.gnu.linkonce.t*)
	}

    .rodata : {
		*(.rodata .rodata.*)
		*(.gnu.linkonce.r*)
		. = ALIGN(16);
		__axapi_exports_start = .;
		KEEP(*(.axapi.exports))
		__axapi_exports_end = .;
	}

    PROVIDE(_data = .);
    
	.data : {
		*(.data .data.*)
		*(.gnu.linkonce.d*)
	}

    .bss (NOLOAD) : {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }
    _end = .;

	/DISCARD/ : {
		*(.comment)
		*(.gnu*)
		*(.note*)
		*(.eh_frame*)
	}
}

/*__bss_size = (__bss_end - __bss_start) >> 3;*/
