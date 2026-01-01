#include <acpi/acpi.h>
#include <acpi/hpet.h>
#include <arch/cpu/cpu.h>
// #include <mm/pmm.h>
#include <mm/vmm.h>
#include <aurix.h>
#include <stdint.h>

struct hpet_sdt *hpet_sdt;
struct hpet *hpet;

uint64_t period = 0;

uint64_t hpet_get_ns(void)
{
	return hpet->main_count_val * period;
}

void hpet_msleep(uint64_t ms)
{
	hpet_nsleep(ms * 1000000);
}

void hpet_nsleep(uint64_t ns)
{
	uint64_t start = hpet_get_ns();
	uint64_t target = start + ns;
	uint64_t cur = hpet_get_ns();

	while (cur <= target) {
		cur = hpet_get_ns();
		cpu_nop();
	}
}

void acpi_hpet_init()
{
	hpet_sdt = (struct hpet_sdt *)find_sdt("HPET");
	info("HPET Address: 0x%llx\n", (void *)hpet_sdt->addr.addr);

	map_page(NULL, PHYS_TO_VIRT(hpet_sdt->addr.addr), hpet_sdt->addr.addr,
			 VMM_PRESENT | VMM_WRITABLE | VMM_CACHE_DISABLE | VMM_WRITETHROUGH);
	hpet = (struct hpet *)PHYS_TO_VIRT(hpet_sdt->addr.addr);

	if (!(hpet->general_caps & (1 << 15))) {
		error("HPET is not capable of acting as legacy replacement\n");
		hpet = NULL;
		return;
	}

	uint64_t countclk_period = hpet->general_caps >> 32;
	uint64_t freq = 1000000000000000 / countclk_period;

	period = countclk_period / 1000000;

	hpet->general_conf = hpet->general_conf | 1;

	//const char prefixes[] = {'k', 'M', 'G'};
	//char prefix = 'n';
	//int i = 0;
	//while (((freq % 1000) == 0) && (i != 2)) {
	//    prefix = prefixes[i];
	//    freq /= 1000;
	//}

	info("Enabled HPET with frequency of %uHz\n", freq);
}