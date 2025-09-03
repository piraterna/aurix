/*********************************************************************************/
/* Module Name:  kinit.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#include <boot/aurix.h>
#include <cpu/cpu.h>
#include <debug/uart.h>

void _start(struct aurix_parameters *params)
{
	serial_init();

	if (params->revision != AURIX_PROTOCOL_REVISION) {
		serial_sendstr("Aurix Protocol revision is not compatible!\n");
	}

	serial_sendstr("Hello from AurixOS!\n");

	// initialize basic processor features and interrupts
	cpu_early_init();

	__asm__ volatile("int $0x20");

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("cli;hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}
}