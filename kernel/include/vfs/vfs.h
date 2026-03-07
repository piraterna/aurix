/*********************************************************************************/
/* Module Name:  vfs.h */
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

#ifndef _VFS_VFS_H
#define _VFS_VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <vfs/fileio.h>

struct vnode;
struct vfs;

#define V_CREATE (1 << 0)

enum vnodeype {
	VNODE_NULL,
	VNODE_REGULAR,
	VNODE_DIR,
	VNODE_BLOCK,
	VNODE_CHAR,
	VNODE_LINK,
	VNODE_PIPE,
	VNODE_SOCKET,
	VNODE_BAD,
};

struct statfs {
	uint64_t fsid;

	uint64_t block_size;
	uint64_t total_blocks;
	uint64_t free_blocks;

	uint64_t total_nodes;
	uint64_t free_nodes;
};

struct fid {
	size_t fid_len;
	char *fid_data;
};

struct vfs_fops {
	int (*mount)(struct vfs *, char *, void *);
	int (*unmount)(struct vfs *);

	int (*lookup)(struct vfs *, char *);

	int (*root)(struct vfs *, struct vnode **);

	int (*statfs)(struct vfs *, struct statfs *);

	int (*sync)(struct vfs *);

	int (*fid)(struct vfs *, struct vnode *, struct fid *);
	int (*vget)(struct vfs *, struct vnode **, struct fid *);
};

struct vfs_vops {
	int (*open)(struct vnode **, int, bool, struct fileio **);
	int (*close)(struct vnode *, int, bool);

	int (*read)(struct vnode *, size_t *, size_t *, void *);
	int (*write)(struct vnode *, void *, size_t *, size_t *);
	int (*ioctl)(struct vnode *, int, void *);
};

struct vnode {
	char *path;
	uint8_t vtype;
	void *node_data;

	struct vfs_vops *ops;

	struct vfs *vfs_mount;
	struct vfs *vfs_root;
};

struct vfs {
	struct vnode *root_vnode;
	void *fs_data;

	struct vfs_fops *ops;
	struct vfs *next;
};

struct vfs_fs {
	const char *name;
	int (*create_mount)(struct vfs_fs *fs, struct vfs *mount);
};

struct vfs_file {
	struct vnode *vnode;
	struct vfs_fops fops;

	size_t pos;
	int flags;
};

struct vfs *vfs_create(void *fs_data);
struct vfs *vfs_mount(void *fs, char *path, void *rootvn_data);

int vfs_append(struct vfs *vfs);
struct vnode *vnode_create(struct vfs *root_vfs, char *path, void *data);

int vfs_resolve_mount(char *path, struct vfs **out);

int vfs_open(struct vfs *vfs, char *path, int flags, struct fileio **out);
int vfs_read(struct vnode *vnode, size_t size, size_t offset, void *out);
int vfs_write(struct vnode *vnode, void *buf, size_t size, size_t offset);
int vfs_ioctl(struct vnode *vnode, int request, void *arg);
int vfs_close(struct vnode *vnode);

#endif /* _VFS_VFS_H */