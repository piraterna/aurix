/*********************************************************************************/
/* Module Name:  idt.c */
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

#include <arch/cpu/idt.h>
#include <debug/uart.h>
#include <stdint.h>

#define IDT_TRAP 0xF
#define IDT_INTERRUPT 0xE

void isr_common_handler();

__attribute__((aligned(16))) struct idt_descriptor idt[256];

struct idtr idtr = { .limit = sizeof(idt) - 1, .base = (uint64_t)&idt };

extern void *isr_stubs[256];
void *irq_handlers[16] = { 0 };

void idt_init()
{
	for (int v = 0; v < 32; v++) {
		idt_set_desc(&idt[v], (uint64_t)isr_stubs[v], IDT_TRAP, 0);
	}
	for (int v = 32; v < 256; v++) {
		idt_set_desc(&idt[v], (uint64_t)isr_stubs[v], IDT_INTERRUPT, 0);
	}

	__asm__ volatile("lidt %0; sti" ::"m"(idtr));
}

void idt_set_desc(struct idt_descriptor *desc, uint64_t offset, uint8_t type,
				  uint8_t dpl)
{
	desc->base_low = offset & 0xFFFF;
	desc->codeseg = 0x08;
	desc->flags = (1 << 7) | (dpl << 5) | (type);
	desc->base_mid = (offset >> 16) & 0xFFFF;
	desc->base_high = (offset >> 32) & 0xFFFFFFFF;
	desc->reserved = 0;
}

void isr_common_handler()
{
	serial_sendstr("Received interrupt");
}