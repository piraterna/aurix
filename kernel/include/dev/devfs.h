#ifndef _DEV_DEVFS_H
#define _DEV_DEVFS_H

#include <dev/chrdev.h>
#include <stddef.h>

void devfs_init(void);

int devfs_register(const char *name, const struct chrdev_ops *ops, void *ctx);

int devfs_read(const char *name, void *buf, size_t len);
int devfs_write(const char *name, const void *buf, size_t len);

int devfs_exists(const char *name);

#endif
