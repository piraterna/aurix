#include <sys/ksyms.h>

#include <stddef.h>

__attribute__((weak)) const uint32_t __ksym_count = 0;
__attribute__((weak)) const uint64_t __ksym_addrs[1] = { 0 };
__attribute__((weak)) const uint32_t __ksym_name_offs[1] = { 0 };
__attribute__((weak)) const char __ksym_names[1] = { 0 };

bool ksym_lookup(uintptr_t addr, const char **name_out, uintptr_t *sym_addr_out)
{
	if (!name_out)
		return false;

	*name_out = NULL;
	if (sym_addr_out)
		*sym_addr_out = 0;

	uint32_t count = __ksym_count;
	if (count == 0)
		return false;

	uint32_t lo = 0;
	uint32_t hi = count;
	while (lo < hi) {
		uint32_t mid = lo + (hi - lo) / 2;
		uint64_t mid_addr = __ksym_addrs[mid];
		if (mid_addr <= addr)
			lo = mid + 1;
		else
			hi = mid;
	}

	if (lo == 0)
		return false;

	uint32_t idx = lo - 1;
	const char *name = __ksym_names + __ksym_name_offs[idx];
	*name_out = name;
	if (sym_addr_out)
		*sym_addr_out = (uintptr_t)__ksym_addrs[idx];
	return true;
}
