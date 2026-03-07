#include <vfs/vfs.h>
#include <mm/heap.h>
#include <sys/panic.h>
#include <aurix.h>
#include <stddef.h>
#include <lib/string.h>

struct vfs *rootfs = NULL;

struct vfs *vfs_create(void *fs_data)
{
	struct vfs *v = kmalloc(sizeof(struct vfs));
	memset(v, 0, sizeof(struct vfs));

	// v->fs_type = fs_type;
	v->fs_data = fs_data;

	v->ops = kmalloc(sizeof(struct vfs_fops));
	memset(v->ops, 0, sizeof(struct vfs_fops));

	vfs_append(v);
	return v;
}

struct vfs *vfs_mount(void *fs, char *path, void *rootvn_data)
{
	if (!fs) {
		error("fs pointer is null\n");
		return NULL;
	}

	struct vfs *v = vfs_create(fs);
	struct vfs *rootvfs;
	if (!vfs_resolve_mount(path, &rootvfs)) {
		rootvfs = v;
	}

	v->root_vnode = vnode_create(rootvfs, path, rootvn_data);
	v->root_vnode->vfs_mount = v;

	return v;
}

int vfs_append(struct vfs *vfs)
{
	if (!rootfs) {
		rootfs = vfs;
		return 0;
	}

	struct vfs *v;
	for (v = rootfs; v->next != NULL; v = v->next)
		;

	v->next = vfs;
	return 0;
}

struct vnode *vnode_create(struct vfs *root_vfs, char *path, void *data)
{
	struct vnode *vnode = kmalloc(sizeof(vnode));
	memset(vnode, 0, sizeof(struct vnode));

	vnode->path = kmalloc(strlen(path));
	strcpy(vnode->path, path);

	vnode->vfs_root = root_vfs;
	vnode->node_data = data;

	vnode->ops = kmalloc(sizeof(struct vfs_vops));
	memset(vnode->ops, 0, sizeof(struct vfs_vops));

	return vnode;
}

int vfs_resolve_mount(char *path, struct vfs **out)
{
	struct vfs *v;
	for (v = rootfs; v != NULL; v = v->next) {
		if (!v->root_vnode) {
			return -1;
		}

		char *prefix = v->root_vnode->path;

		if (strlen(path) < strlen(prefix)) {
			continue;
		}

		if (strncmp(path, prefix, strlen(prefix)) == 0) {
			*out = v;
			break;
		}
	}

	if (!v) {
		return -1;
	}

	return 0;
}

int vfs_open(struct vfs *vfs, char *path, int flags, struct fileio **out)
{
	struct vnode *vn_file = vnode_create(vfs, path, NULL);
	vn_file->vfs_mount = vfs;
	memcpy(vn_file->ops, vfs->root_vnode->ops, sizeof(struct vfs_vops));
	vn_file->path += strlen(vfs->root_vnode->path);

	struct fileio *fio_file = fio_create();

	if (!vn_file->ops->open(&vn_file, flags, false, &fio_file)) {
		kfree(vn_file->path);
		kfree(vn_file);
		return -1;
	}

	fio_file->private = vn_file;

	*out = fio_file;

	vn_file->path -= strlen(vfs->root_vnode->path);

	return 0;
}

int vfs_read(struct vnode *vnode, size_t size, size_t offset, void *out)
{
	if (!vnode) {
		return -1;
	}

	return vnode->ops->read(vnode, &size, &offset, out);
}

int vfs_write(struct vnode *vnode, void *buf, size_t size, size_t offset)
{
	if (!vnode) {
		return -1;
	}

	return vnode->ops->read(vnode, buf, &size, &offset);
}

int vfs_ioctl(struct vnode *vnode, int request, void *arg)
{
	if (!vnode) {
		return -1;
	}

	return vnode->ops->ioctl(vnode, request, arg);
}

int vfs_close(struct vnode *vnode)
{
	if (!vnode) {
		return -1;
	}

	int ret = vnode->ops->close(vnode, 0, false);
	if (!ret) {
		return ret;
	}

	kfree(vnode);
	return 0;
}