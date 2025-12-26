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
#include <lib/string.h>
#include <config.h>
#include <aurix.h>
#include <stdint.h>
#include <debug/log.h>

struct madt *madt = NULL;

#if defined(__x86_64__)
uintptr_t lapic_base = 0;

struct madt_lapic *lapics[CONFIG_CPU_MAX_COUNT];
struct madt_ioapic *ioapics[CONFIG_IOAPIC_MAX_COUNT];

size_t lapic_count = 0;
size_t ioapic_count = 0;

struct madt_iso *isos[16];
size_t iso_count = 0;

struct madt_lapic_nmi *nmis[224];
size_t nmi_count = 0;
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
	case MADT_LAPIC_OVERRIDE:
		return "lapic address override";
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

	// DEBUG_POINT
	return;

#if defined(__x86_64__) || defined(__i686__)
	if (madt->flags & 1) {
		debug("Masking 8259 PIC vectors\n");
		outb(0x21, 0xff);
		outb(0xa1, 0xff);
	}

	lapic_base = madt->lapic_addr;
#endif

	for (uint64_t i = 0; i < (madt->hdr.len - sizeof(struct madt));) {
		struct madt_header *mhdr = (struct madt_header *)(madt->structures + i);
		switch (mhdr->type) {
#if defined(__x86_64__)
		case MADT_LAPIC: {
			struct madt_lapic *lapic =
				(struct madt_lapic *)(madt->structures + i);
			if (lapic_count >= CONFIG_CPU_MAX_COUNT) {
				error(
					"Reached maximum allowed CPUs, processor #%u will be left disabled.\n",
					lapic->id);
				break;
			}
			lapics[lapic_count++] = lapic;
			debug("Registered LAPIC for processor #%u with _UID %u (%s)\n",
				  lapic->id, lapic->uid,
				  (lapic->flags & 1) ? "enabled" : "disabled");
			break;
		}
		case MADT_IOAPIC: {
			struct madt_ioapic *ioapic =
				(struct madt_ioapic *)(madt->structures + i);
			if (ioapic_count >= CONFIG_IOAPIC_MAX_COUNT) {
				error(
					"Reached maximum allowed IOAPIC controllers, IOAPIC #%u will be unused.\n",
					ioapic->id);
				break;
			}
			ioapics[ioapic_count++] = ioapic;
			debug("Registered IOAPIC #%u located at 0x%llx (gsi base=%llx)\n",
				  ioapic->id, ioapic->addr, ioapic->gsi_base);
			break;
		}
		case MADT_ISO: {
			struct madt_iso *iso = (struct madt_iso *)(madt->structures + i);
			isos[iso_count++] = iso;
			debug(
				"Interrupt source override on bus %u with source %u (gsi=%u, flags=%x)\n",
				iso->bus, iso->src, iso->gsi, iso->flags);
			break;
		}
		// case MADT_NMI_SRC:
		case MADT_LAPIC_NMI: {
			struct madt_lapic_nmi *nmi =
				(struct madt_lapic_nmi *)(madt->structures + i);
			nmis[nmi_count++] = nmi;
			debug("NMI for LINT#%u on processor with _UID %u, flags %x\n",
				  nmi->LINTn, nmi->acpi_uid, nmi->flags);
			break;
		}
		case MADT_LAPIC_OVERRIDE: {
			struct madt_lapic_override *override =
				(struct madt_lapic_override *)(madt->structures + i);
			lapic_base = override->addr;
			debug("Overridden LAPIC base address: 0x%llx\n", override->addr);
			break;
		}
		case MADT_NMI_SRC:
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
			error("Unhandled MADT Entry with type %u (%s)\n", mhdr->type,
				  madt_type_to_str(mhdr->type));
			break;
		}

		i += mhdr->len;
	}
}
