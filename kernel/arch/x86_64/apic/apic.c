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
	lapic_write(APIC_EOI, 0);
}

void apic_init()
{
	// initialize lapic
	map_page(NULL, PHYS_TO_VIRT(lapic_base), lapic_base, VMM_PRESENT | VMM_WRITABLE | VMM_WRITETHROUGH | VMM_CACHE_DISABLE);
	lapic_base = PHYS_TO_VIRT(lapic_base);

	// initialize ioapic
	for (size_t i = 0; i < ioapic_count; i++) {
		debug("Mapping I/O APIC #%u (0x%llx -> 0x%llx)...\n", i, ioapics[i]->addr, PHYS_TO_VIRT(ioapics[i]->addr));
		map_page(NULL, PHYS_TO_VIRT(ioapics[i]->addr), ioapics[i]->addr, VMM_PRESENT | VMM_WRITABLE | VMM_WRITETHROUGH | VMM_CACHE_DISABLE);

		// mask interrupts
		uint8_t maxreds = (ioapic_read(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICVER) >> 16) & 0xFF;
		debug("Initializing %u masked interrupts for I/O APIC #%u...\n", maxreds, i);
		
		for (int n = 0; n < maxreds; n++) {
			ioapic_write(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICREDTBLL(n), (1 << 16));
			ioapic_write(PHYS_TO_VIRT(ioapics[i]->addr), IOAPICREDTBLH(n), 0);
		}
	}

	// enable apic
	lapic_write(APIC_SPURIOUS_IVR, 0x1FF);
	debug("Enabled APIC\n");
}