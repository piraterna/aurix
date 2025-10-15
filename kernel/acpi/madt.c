/*********************************************************************************/
/* Module Name:  madt.c */
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

#if defined(__x86_64__) || defined(__i686__)
#include <arch/cpu/cpu.h>
#endif
#include <acpi/acpi.h>
#include <acpi/madt.h>
#include <debug/log.h>
#include <lib/string.h>
#include <stdint.h>

struct madt *madt = NULL;

// TODO: Add KConfig
#define CONFIG_CPU_MAX 64
#define CONFIG_IOAPIC_MAX 4

#if defined(__x86_64__)
struct madt_lapic *lapics[CONFIG_CPU_MAX];
struct madt_ioapic *ioapics[CONFIG_IOAPIC_MAX];
#endif

static char *madt_type_to_str(uint8_t type)
{
	switch (type) {
#if defined(__x86_64__)
		case MADT_LAPIC:
			return "apic";
		case MADT_IOAPIC:
			return "ioapic";
		case MADT_ISO:
			return "interrupt source override";
		case MADT_NMI_SRC:
			return "non-maskable interrupt source";
		case MADT_LAPIC_NMI:
			return "non-maskable interrupt";
		case MADT_LAPIC_NMI_OVERRIDE:
			return "non-maskable interrupt override";
		case MADT_LX2APIC:
			return "local x2apic";
		case MADT_LX2APIC_NMI:
			return "local x2apic nmi";
#elif defined(__aarch64__)
		case MADT_GICC:
			return "gic cpu interface";
		case MADT_GICD:
			return "gic distributor";
		case MADT_GIC_MSI_FRAME:
			return "gic msi frame";
		case MADT_GICR:
			return "gic redistributor";
		case MADT_GIC_ITS:
			return "gic interrupt translation service";
#endif
		default:
			return "unknown";
	}
}

void acpi_madt_init()
{
	madt = (struct madt *)find_sdt("APIC");
	info("MADT Address: 0x%llx\n", (void *)madt);

#if defined(__x86_64__) || defined(__i686__)
	if (madt->flags & 1) {
		debug("Masking 8259 PIC vectors\n");
		outb(0x21, 0xff);
		outb(0xa1, 0xff);
	}
#endif

	for (uint64_t i = 0; i < (madt->hdr.len - sizeof(struct madt));) {
		struct madt_header *mhdr = (struct madt_header *)(madt->structures + i);
		switch (mhdr->type) {
#if defined(__x86_64__)
			case MADT_LAPIC:
			case MADT_IOAPIC:
			case MADT_ISO:
			case MADT_NMI_SRC:
			case MADT_LAPIC_NMI:
			case MADT_LAPIC_NMI_OVERRIDE:
			case MADT_LX2APIC:
			case MADT_LX2APIC_NMI:
#elif defined(__aarch64__)
			case MADT_GICC:
			case MADT_GICD:
			case MADT_GIC_MSI_FRAME:
			case MADT_GICR:
			case MADT_GIC_ITS:
#endif
			default:
				error("Unhandled MADT Entry with type %u (%s)\n", mhdr->type, madt_type_to_str(mhdr->type));
				break;
		}

		i += mhdr->len;
	}
}