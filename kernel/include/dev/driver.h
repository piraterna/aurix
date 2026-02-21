#ifndef _DEV_DRIVER_H
#define _DEV_DRIVER_H

#include <dev/device.h>

typedef int (*driver_probe_fn)(struct device *dev);
typedef void (*driver_remove_fn)(struct device *dev);

struct driver {
	const char *name;
	const char *class_name;
	driver_probe_fn probe;
	driver_remove_fn remove;
};

void driver_core_init(void);
int driver_register(struct driver *drv);
int driver_bind_all(void);
int driver_is_ready(const char *driver_name);

int driver_exists(const char *driver_name);

#endif
