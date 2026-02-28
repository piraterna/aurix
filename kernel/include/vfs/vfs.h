#ifndef _VFS_VFS_H
#define _VFS_VFS_H

#include <stdint.h>
#include <stddef.h>

struct vnode;
struct vfs_mount;
struct vfs_fs;
struct vfs_file;

struct vfs_fops {
	int (*write)(struct vfs_file *file, const void *buf, size_t len);
	int (*read)(struct vfs_file *file, void *buf, size_t len);
};

struct vfs_vops {
	int (*lookup)(struct vnode *node, struct vnode **target,
				  const char *component_name);
	int (*create)(struct vnode *node, struct vnode **target,
				  const char *component_name);
};

struct vnode {
	struct vfs_mount *mount;
	struct vfs_vops *vops;
	struct vfs_fileops *fops;

	void *data;
};

struct vfs_mount {
	struct vnode *root;
	struct vfs_fs *fs;
};

struct vfs_fs {
	const char *name;
	int (*create_mount)(struct vfs_fs *fs, struct vfs_mount *mount);
};

struct vfs_file {
	struct vnode *vnode;
	struct vfs_fops fops;

	size_t pos;
	int flags;
};

void vfs_init(void);

int vfs_registerfs(struct vfs_fs *fs);

struct vfs_file *vfs_open(const char *pathname, int flags);
int vfs_close(struct vfs_file *file);
int vfs_write(struct vfs_file *file, const void *buf, size_t len);
int vfs_read(struct vfs_file *file, void *buf, size_t len);

#endif /* _VFS_VFS_H */