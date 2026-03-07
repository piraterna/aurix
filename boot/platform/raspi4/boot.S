/*********************************************************************************/
/* Module Name:  boot.S                                                          */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy                                            */
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

.section ".text.boot"

.global _start

_start:
    //
	// hello, am i on the main core?
	//
    mrs x1, mpidr_el1
    and x1, x1, #3
    cbz x1, 2f
	
	//
    // no? alright then, sorry for interrupting. *leaves*
	//
1:
	wfe
    b 1b

	//
	// ok cool now execute this huge pile of horrible code
	// thanks :>
	//
2:
    //
	// let the stack live below our code
	//
    ldr x1, =_start
    mov sp, x1

	//
	// no junk allowed in .bss!
	//
    ldr x1, =__bss_start
    ldr w2, =__bss_size
3:  cbz w2, 4f
    str xzr, [x1], #8
    sub w2, w2, #1
    cbnz w2, 3b

4:
	bl axboot_init
    
	//
	// crazy? i was crazy once.
	// they locked me in a room. a rubber room. a rubber room with rats.
	// and rats make me crazy.
	// (bootloader returned, just halt the whole thing)
	//
    b 1b
