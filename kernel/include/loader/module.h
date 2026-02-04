#ifndef _LOADER_MODULE_H
#define _LOADER_MODULE_H

#include <stdint.h>
#include <stdbool.h>

bool module_load(void *addr, uint32_t size);

#endif /* _LOADER_MODULE_H */