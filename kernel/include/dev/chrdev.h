#ifndef _DEV_CHRDEV_H
#define _DEV_CHRDEV_H

#include <stddef.h>

struct chrdev_ops {
	int (*read)(void *ctx, void *buf, size_t len);
	int (*write)(void *ctx, const void *buf, size_t len);
};

#endif
