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

typedef enum devfs_ftype {
	DEVFS_TYPE_DIR,
	DEVFS_TYPE_CHAR,
	DEVFS_TYPE_BLOCK,
	DEVFS_TYPE_FILE,
} devfs_ftype_t;

typedef struct devfs_node {
	char *name;

	devfs_ftype_t type;

	struct device *device;

	struct devfs_node *sibling;
	struct devfs_node *child;
} devfs_node_t;

typedef struct devfs {
	struct devfs_node *root_node;

	size_t devfs_size; // node count?
} devfs_t;

extern struct devfs *global_devfs;

devfs_t *devfs_create();
devfs_node_t *devfs_create_node(devfs_ftype_t ftype);
int devfs_find_node(devfs_t *devfs, char *path, devfs_node_t **out);
int devfs_find_or_create_node(devfs_t *ramfs, char *path,
							  devfs_ftype_t ramfs_ftype, devfs_node_t **out);

int devfs_append_child(devfs_node_t *parent, devfs_node_t *child);
int devfs_node_add(devfs_t *ramfs, char *path, devfs_node_t **out);

int devfs_vfs_init(devfs_t *ramfs, char *mount_path);

int devfs_print(devfs_node_t *devfs, int lvl);

int devfs_open(struct vnode **vnode_r, int flags, bool clone,
			   struct fileio **fio_out);
int devfs_close(struct vnode *vnode, int flags, bool clone);

int devfs_ioctl(struct vnode *vnode, int request, void *arg);
int devfs_read(struct vnode *vn, size_t *bytes, size_t *offset, void *out);
int devfs_write(struct vnode *vn, void *buf, size_t *bytes, size_t *offset);

int devfs_refresh(devfs_t *devfs);

#endif // _FS_DEVFS_H