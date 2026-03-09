/*********************************************************************************/
/* Module Name:  devfs.h */
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

#ifndef _FS_DEVFS_H
#define _FS_DEVFS_H

#include <dev/device.h>
#include <vfs/fileio.h>
#include <vfs/vfs.h>

enum devfs_ftype {
	DEVFS_TYPE_DIR,
	DEVFS_TYPE_CHAR,
	DEVFS_TYPE_BLOCK,
	DEVFS_TYPE_FILE,
};

struct devfs_node {
	char *name;

	enum devfs_ftype type;

	struct device *device;

	struct devfs_node *sibling;
	struct devfs_node *child;
};

struct devfs {
	struct devfs_node *root_node;

	size_t devfs_size; // node count?
};

extern struct devfs *global_devfs;
extern struct vfs_vops devfs_vnops;

struct devfs *devfs_create();
struct devfs_node *devfs_create_node(enum devfs_ftype ftype);
int devfs_find_node(struct devfs *devfs, char *path, struct devfs_node **out);
int devfs_find_or_create_node(struct devfs *ramfs, char *path,
							  enum devfs_ftype ramfs_ftype,
							  struct devfs_node **out);

int devfs_append_child(struct devfs_node *parent, struct devfs_node *child);
int devfs_node_add(struct devfs *ramfs, char *path, struct devfs_node **out);

int devfs_vfs_init(struct devfs *ramfs, char *mount_path);

int devfs_print(struct devfs_node *devfs, int lvl);

int devfs_open(struct vnode **vnode_r, int flags, bool clone,
			   struct fileio **fio_out);
int devfs_close(struct vnode *vnode, int flags, bool clone);

int devfs_ioctl(struct vnode *vnode, int request, void *arg);
int devfs_read(struct vnode *vn, size_t *bytes, size_t *offset, void *out);
int devfs_write(struct vnode *vn, void *buf, size_t *bytes, size_t *offset);

int devfs_refresh(struct devfs *devfs);

#endif // _FS_DEVFS_H