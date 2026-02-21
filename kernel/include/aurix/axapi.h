#ifndef _AURIX_AXAPI_H
#define _AURIX_AXAPI_H

#include <stddef.h>
#include <stdint.h>

struct chrdev_ops;
struct device;
struct driver;

struct axapi_export {
	const char *name;
	uintptr_t addr;
} __attribute__((packed));

struct axapi_import {
	const char *name;
	uintptr_t *target;
} __attribute__((packed));

#ifdef __KERNEL__
#define AXAPI_SYM(ret, name, args) ret name args;
#else
#ifdef AXAPI_DEFINE
#define AXAPI_SYM(ret, name, args)                                             \
	ret(*name) args = 0;                                                       \
	__attribute__((section(".axapi.imports"),                                  \
				   used)) static struct axapi_import __axapi_import_##name = { \
		#name, (uintptr_t *)&name                                              \
	};
#else
#define AXAPI_SYM(ret, name, args) extern ret(*name) args;
#endif
#endif

#include <aurix/axapi_defs.inc>

#undef AXAPI_SYM

#endif /* _AURIX_AXAPI_H */
