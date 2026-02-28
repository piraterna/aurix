#include <vfs/vfs.h>
#include <mm/heap.h>
#include <sys/panic.h>
#include <aurix.h>
#include <stddef.h>

struct vfs_mount *rootfs = NULL;

void vfs_init()
{
	rootfs = kmalloc(sizeof(struct vfs_mount));
	if (!rootfs) {
		kpanic(NULL, "Failed to allocate memory for RootFS structure");
	}
}

int vfs_registerfs(struct vfs_fs *fs)
{
	(void)fs;
	return 0;
}

struct vfs_file *vfs_open(const char *pathname, int flags)
{
	(void)pathname;
	(void)flags;
	return NULL;
}

int vfs_close(struct vfs_file *file)
{
	(void)file;
	return 0;
}

int vfs_write(struct vfs_file *file, const void *buf, size_t len)
{
	(void)file;
	(void)buf;
	(void)len;
	return 0;
}

int vfs_read(struct vfs_file *file, void *buf, size_t len)
{
	(void)file;
	(void)buf;
	(void)len;
	return 0;
}