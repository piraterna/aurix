#ifndef _DEV_DEVICE_H
#define _DEV_DEVICE_H

#include <stdint.h>

struct driver;

struct device {
	const char *name;
	const char *class_name;
	void *driver_data;
	struct driver *bound_driver;
};

int device_register(struct device *dev);

#endif
