#include <acpi/acpi.h>
#include <acpi/hpet.h>
// #include <mm/pmm.h>
#include <mm/vmm.h>
#include <aurix.h>
#include <stdint.h>

struct hpet_sdt *hpet_sdt;
struct hpet *hpet;

uint64_t period = 0;

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