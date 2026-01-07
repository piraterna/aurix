#include <arch/apic/apic.h>
#include <arch/cpu/cpu.h>
#include <mm/vmm.h>
#include <acpi/madt.h>
#include <aurix.h>
#include <stdint.h>

#define IOREGSEL 0x00
#define IOWIN 0x10

extern uintptr_t lapic_base;

extern struct madt_lapic *lapics[];
extern struct madt_ioapic *ioapics[];

extern size_t lapic_count;
extern size_t ioapic_count;

extern struct madt_iso *isos[16];
extern size_t iso_count;

extern struct madt_lapic_nmi *nmis[224];
extern size_t nmi_count;

uint64_t apic_msr_read(uint64_t offset)
{
	return rdmsr(APIC_BASE_MSR + offset);
}

void apic_msr_write(uint64_t offset, uint64_t val)
{
	wrmsr(APIC_BASE_MSR + offset, val);
}

uint32_t ioapic_read(uintptr_t base, uint8_t regoff)
{
	*(volatile uint32_t *)(base + IOREGSEL) = regoff;
	return *(volatile uint32_t *)(base + IOWIN);
}

void ioapic_write(uintptr_t base, uint8_t regoff, uint32_t data)
{
	*(volatile uint32_t *)(base + IOREGSEL) = regoff;
	*(volatile uint32_t *)(base + IOWIN) = data;
}

uint32_t lapic_read(uint16_t reg)
{
	return *(volatile uint32_t *)(lapic_base + reg);
}

void lapic_write(uint16_t reg, uint32_t val)
{
	*(volatile uint32_t *)(lapic_base + reg) = val;
}

void apic_send_eoi()
{
	lapic_write(APIC_EOI, 12);
}

void ioapic_write_red(uint32_t gsi, uint8_t vec, uint8_t delivery_mode,
					  uint8_t polarity, uint8_t trigger_mode, uint8_t lapic_id)
{
	union ioapic_redirect_entry redent = { 0 };
	redent.vec = vec;
	redent.delivery_mode = delivery_mode;
	redent.delivery_status = 0;
	redent.pin_polarity = polarity;
	redent.remote_irr = 0;
	redent.trigger_mode = trigger_mode;
	redent.mask = 0;
	redent.reserved = 0;
	redent.dest = lapic_id;

	size_t i;
	for (i = 0; i < iso_count; i++) {
		if ((vec - 32) == isos[i]->src) {
			gsi = isos[i]->gsi;

			if (isos[i]->flags & 2) {
				redent.pin_polarity = IOAPIC_ACTIVE_LO;
			}
			if (isos[i]->flags & 8) {
				redent.trigger_mode = IOAPIC_TRIGGER_LEVEL;
			}
			break;
		}
	}

	bool found = false;
	for (i = 0; i < ioapic_count; i++) {
		uint8_t maxreds =
			(ioapic_read(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICVER) >> 16) &
			0xFF;
		if (ioapics[i]->gsi_base <= gsi &&
			ioapics[i]->gsi_base + maxreds > gsi) {
			found = true;
			break;
		}
	}

	if (!found) {
		error("Couldn't find IOAPIC for GSI %u\n", gsi);
		return;
	}

	uint32_t pin = gsi - ioapics[i]->gsi_base;

	ioapic_write(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICREDTBLL(pin),
				 redent.bytes.low);
	ioapic_write(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICREDTBLH(pin),
				 redent.bytes.high);

	trace("Set IOAPIC redirection entry for vector %u, gsi %u (0x%lx%lx)\n",
		  vec, gsi, redent.bytes.high, redent.bytes.low);
}

void apic_init()
{
	// initialize lapic
	map_page(NULL, PHYS_TO_VIRT(lapic_base), lapic_base,
			 VMM_PRESENT | VMM_WRITABLE | VMM_WRITETHROUGH | VMM_CACHE_DISABLE);
	lapic_base = PHYS_TO_VIRT(lapic_base);

	uint64_t lapicMsr = rdmsr(0x1b);
	wrmsr(0x1b, (lapicMsr | 0x800) & ~0x100);

	lapic_write(0xf0, lapic_read(0xf0) | 0x100);

	lapic_write(0x320, 0x10000);
	lapic_write(0x330, (1 << 16));
	lapic_write(0x340, (1 << 16));
	lapic_write(0x350, (1 << 16));
	lapic_write(0x360, (1 << 16));
	lapic_write(0x370, (1 << 16));

	lapic_write(0x80, 0);

	// initialize ioapic
	for (size_t i = 0; i < ioapic_count; i++) {
		info("Mapping I/O APIC #%u (0x%llx -> 0x%llx)...\n", i,
			  ioapics[i]->addr, PHYS_TO_VIRT(ioapics[i]->addr));
		map_page(NULL, PHYS_TO_VIRT(ioapics[i]->addr), ioapics[i]->addr,
				 VMM_PRESENT | VMM_WRITABLE | VMM_WRITETHROUGH |
					 VMM_CACHE_DISABLE);

		// mask interrupts
		uint8_t maxreds =
			(ioapic_read(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICVER) >> 16) &
			0xFF;
		info("Initializing %u masked interrupts for I/O APIC #%u...\n",
			  maxreds, i);

		for (int n = 0; n < maxreds; n++) {
			ioapic_write(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICREDTBLL(n),
						 (1 << 16));
			ioapic_write(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICREDTBLH(n), 0);
		}
	}

	// enable apic
	lapic_write(APIC_SPURIOUS_IVR, lapic_read(APIC_SPURIOUS_IVR) | 0x100);
	debug("Enabled APIC\n");
}