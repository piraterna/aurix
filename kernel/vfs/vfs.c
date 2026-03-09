/*********************************************************************************/
/* Module Name:  vfs.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

// This code was originally from https://github.com/purpleK2/kernel
// Licensed under the MIT License.

#include <vfs/vfs.h>
#include <mm/heap.h>
#include <sys/panic.h>
#include <aurix.h>
#include <stddef.h>
#include <lib/string.h>
#include <debug/assert.h>

struct vfs *vfs_list = NULL;

struct vfs *vfs_create(void *fs_data)
{
	struct vfs *v = kmalloc(sizeof(struct vfs));
	memset(v, 0, sizeof(struct vfs));

	v->ops = kmalloc(sizeof(struct vfs_fops));
	memset(v->ops, 0, sizeof(struct vfs_fops));

	v->fs_data = fs_data;

	vfs_append(v);

	return v;
}

struct vfs *vfs_mount(void *fs, char *path, void *rootvn_data)
{
	trace("vfs_mount: fs=%p path=%s\n", fs, path);
	if (!fs) {
		error("fs pointer is null\n");
		return NULL;
	}

	struct vfs *v = vfs_create(fs);
	struct vfs *rootvfs;
	if (vfs_resolve_mount(path, &rootvfs) != 0) {
		rootvfs = v;
	}

	v->root_vnode = vnode_create(rootvfs, path, rootvn_data);
	trace("vfs_mount: root_vnode->path=%s\n", v->root_vnode->path);
	v->root_vnode->vfs_mount = v;

	return v;
}

int vfs_append(struct vfs *vfs)
{
	if (!vfs_list) {
		vfs_list = vfs;
		return 0;
	}

	struct vfs *v;
	for (v = vfs_list; v->next != NULL; v = v->next)
		;

	v->next = vfs;
	return 0;
}

struct vnode *vnode_create(struct vfs *root_vfs, char *path, void *data)
{
	struct vnode *vnode = kmalloc(sizeof(struct vnode));
	memset(vnode, 0, sizeof(struct vnode));

	vnode->path = strdup(path);

	vnode->vfs_root = root_vfs;
	vnode->node_data = data;

	vnode->ops = kmalloc(sizeof(struct vfs_vops));
	memset(vnode->ops, 0, sizeof(struct vfs_vops));

	return vnode;
}

int vfs_resolve_mount(char *path, struct vfs **out)
{
	struct vfs *v = vfs_list;
	for (; v != NULL; v = v->next) {
		if (!v->root_vnode) {
			error("vfs: No root vnode for vnode\n");
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
		warn("vfs: NULL vfs_list?\n");
		return -1;
	}
	return 0;
}

int vfs_open(struct vfs *vfs, char *path, int flags, struct fileio **out)
{
	trace("vfs_open: vfs=%p path=%s\n", vfs, path);
	struct vnode *vn_file = vnode_create(vfs, path, NULL);
	vn_file->vfs_mount = vfs;
	trace("vfs_open: root_vnode->ops=%p\n", vfs->root_vnode->ops);
	memcpy(vn_file->ops, vfs->root_vnode->ops, sizeof(struct vfs_vops));
	vn_file->path += strlen(vfs->root_vnode->path);

	struct fileio *fio_file = fio_create();

	if (!vn_file->ops->open) {
		warn("open() not present for %s\n", path);
		return -1;
	}

	if (vn_file->ops->open(&vn_file, flags, false, &fio_file) != 0) {
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

	return vnode->ops->write(vnode, buf, &size, &offset);
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