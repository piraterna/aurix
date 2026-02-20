#ifndef _SYS_KSYMS_H
#define _SYS_KSYMS_H

#include <stdbool.h>
#include <stdint.h>

bool ksym_lookup(uintptr_t addr, const char **name_out,
				 uintptr_t *sym_addr_out);

#endif
