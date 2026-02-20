/*********************************************************************************/
/* Module Name:  irq.h */
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

#include <arch/apic/apic.h>
#include <arch/cpu/idt.h>
#include <arch/cpu/irq.h>
#include <arch/cpu/cpu.h>
#include <aurix.h>
#include <stddef.h>

// this probably isn't the best way to do it... beats me
struct irq_handler irq_handlers[224] = { 0 };

void irq_install(uint8_t irq, irq_callback callback, void *ctx)
{
	if (irq > 16) {
		warn("Tried to install an IRQ handler for IRQ#%u.\n", irq);
		return;
	}

	struct irq_handler *h = &irq_handlers[irq];
	if (h->callback) {
		warn("Overwriting IRQ callback 0x%llx for IRQ%u with 0x%llx.\n",
			 h->callback, irq, callback);
	}

	h->callback = callback;
	h->ctx = ctx;

	if (irq == 0)
		ioapic_write_red(irq, 0x20 + irq, 0, 0, 0, 0xFF);
	else
		ioapic_write_red(irq, 0x20 + irq, 0, 0, 0,
					 (uint8_t)(1u << cpu_get_current()->id));

	debug("Installed IRQ handler 0x%llx for IRQ%u.\n", callback, irq);
}

void irq_uninstall(uint8_t irq)
{
	if (irq > 16) {
		warn("Tried to uninstall an IRQ handler for IRQ#%u.\n", irq);
		return;
	}

	struct irq_handler *h = &irq_handlers[irq];

	h->callback = NULL;
	h->ctx = NULL;

	debug("Uninstalled IRQ handler for IRQ#%u.\n", irq);
}

void irq_dispatch(uint8_t irq)
{
	struct irq_handler *h = &irq_handlers[irq];
	if (!h->callback) {
		debug("Unhandled IRQ#%u.\n", irq);
		return;
	}

	h->callback(h->ctx);
}
