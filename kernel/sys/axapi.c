#include <aurix/axapi.h>
#include <sys/axapi.h>

#include <string.h>

extern const struct axapi_export __axapi_exports_start[];
extern const struct axapi_export __axapi_exports_end[];

#undef AXAPI_SYM
#define AXAPI_SYM(ret, name, args)                                        \
	__attribute__((                                                       \
		section(".axapi.exports"),                                        \
		used)) static const struct axapi_export __axapi_export_##name = { \
		#name, (uintptr_t) & name                                         \
	};
#include <aurix/axapi_defs.inc>
#undef AXAPI_SYM

uintptr_t axapi_resolve(const char *name)
{
	if (!name)
		return 0;

	for (const struct axapi_export *e = __axapi_exports_start;
		 e < __axapi_exports_end; e++) {
		if (e->name && strcmp(e->name, name) == 0)
			return e->addr;
	}

	return 0;
}
