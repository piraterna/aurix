/*********************************************************************************/
/* Module Name:  ramfs.h */
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

#ifndef _FS_RAMFS_H
#define _FS_RAMFS_H

#include <vfs/vfs.h>
#include <vfs/fileio.h>
#include <stdbool.h>
#include <stddef.h>

enum ramfs_ftype {
	RAMFS_FILE,
	RAMFS_DIRECTORY,
	RAMFS_SYMLINK,
};

struct ramfs_node {
	char *name;
	enum ramfs_ftype type;
	size_t size;
	void *data;

	int mode;

	struct ramfs_node *sibling;
	struct ramfs_node *child;
};

struct ramfs {
	struct ramfs_node *root_node;
	size_t ramfs_size;
};

struct ramfs *ramfs_create_fs();
struct ramfs_node *ramfs_create_node(enum ramfs_ftype ftype);
int ramfs_find_node(struct ramfs *ramfs, char *path, struct ramfs_node **out);
int ramfs_find_or_create_node(struct ramfs *ramfs, char *path,
							  enum ramfs_ftype ramfs_ftype,
							  struct ramfs_node **out);
int ramfs_append_child(struct ramfs_node *parent, struct ramfs_node *child);
int ramfs_node_add(struct ramfs *ramfs, char *path, struct ramfs_node **out);
int ramfs_print(struct ramfs_node *node, int lvl);

void ramfs_init(void);
int ramfs_vfs_init(struct ramfs *ramfs, char *mount_path);

int ramfs_open(struct vnode **vnode_r, int flags, bool clone,
			   struct fileio **fio_out);
int ramfs_close(struct vnode *vnode, int flags, bool clone);
int ramfs_read(struct vnode *vn, size_t *bytes, size_t *offset, void *out);
int ramfs_write(struct vnode *vn, void *buf, size_t *bytes, size_t *offset);
int ramfs_ioctl(struct vnode *vnode, int request, void *arg);
int ramfs_lookup(struct vnode *parent, const char *name, struct vnode **out);
int ramfs_readdir(struct vnode *vnode, struct dirent *entries, size_t *count);
int ramfs_readlink(struct vnode *vnode, char *buf, size_t size);
int ramfs_mkdir(struct vnode *parent, const char *name, int mode);
int ramfs_rmdir(struct vnode *parent, const char *name);
int ramfs_create(struct vnode *parent, const char *name, mode_t mode,
				 struct vnode **out);
int ramfs_remove(struct vnode *parent, const char *name);
int ramfs_symlink(struct vnode *parent, const char *name, const char *target);

#endif // _FS_RAMFS_H