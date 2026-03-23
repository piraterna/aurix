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
#include <vfs/fsid.h>

#include <sys/types.h>
#include <sys/spinlock.h>

#define V_CREATE (1 << 0)
#define V_READ (1 << 1)
#define V_WRITE (1 << 2)
#define V_EXCL (1 << 3)
#define V_TRUNC (1 << 4)
#define V_DIR (1 << 5)

struct vnode;
struct vfs;

struct vfs_fstype {
	uint16_t id;
	char name[64];

	int (*mount)(void *device, char *mount_point, void *mount_data,
				 struct vfs **out);

	struct vfs_fstype *next;
};

enum vnode_type {
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

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

static inline uint8_t vnode_type_to_dtype(enum vnode_type vt)
{
	switch (vt) {
	case VNODE_REGULAR:
		return DT_REG;
	case VNODE_DIR:
		return DT_DIR;
	case VNODE_BLOCK:
		return DT_BLK;
	case VNODE_CHAR:
		return DT_CHR;
	case VNODE_LINK:
		return DT_LNK;
	case VNODE_PIPE:
		return DT_FIFO;
	case VNODE_SOCKET:
		return DT_SOCK;
	default:
		return DT_UNKNOWN;
	}
}

struct statfs {
	fsid_t fsid;

	uint64_t block_size;
	uint64_t total_blocks;
	uint64_t free_blocks;

	uint64_t total_nodes;
	uint64_t free_nodes;
};

struct stat {
	uint64_t st_dev;
	uint64_t st_ino;
	unsigned long st_nlink;
	mode_t st_mode;
	uid_t st_uid;
	gid_t st_gid;
	unsigned int __pad0;
	uint64_t st_rdev;
	off_t st_size;
	long st_blksize;
	int64_t st_blocks;
	// TODO
	// struct timespec st_atim;
	// struct timespec st_mtim;
	// struct timespec st_ctim;
	uint32_t st_atim;
	uint32_t st_mtim;
	uint32_t st_ctim;
	long __unused[3];
};

struct fid {
	size_t fid_len;
	char *fid_data;
};

struct dirent {
	uint64_t d_ino;
	int64_t d_off;
	uint16_t d_reclen;
	uint8_t d_type;
	char d_name[256];
} __attribute__((packed));

struct vfs_ops {
	int (*mount)(struct vfs *, char *, void *);
	int (*unmount)(struct vfs *);
	int (*root)(struct vfs *, struct vnode **);
	int (*statfs)(struct vfs *, struct statfs *);
	int (*sync)(struct vfs *);
	int (*fid)(struct vfs *, struct vnode *, struct fid **);
	int (*vget)(struct vfs *, struct vnode **, struct fid *);
};

struct vnode_ops {
	int (*open)(struct vnode **, int, bool, struct fileio **);
	int (*close)(struct vnode *, int, bool);

	int (*read)(struct vnode *, size_t *, size_t *, void *);
	int (*write)(struct vnode *, void *, size_t *, size_t *);
	int (*ioctl)(struct vnode *, int, void *);
	int (*lookup)(struct vnode *, const char *, struct vnode **);
	int (*readdir)(struct vnode *, struct dirent *, size_t *);
	int (*readlink)(struct vnode *, char *, size_t);
	int (*mkdir)(struct vnode *, const char *, int);
	int (*rmdir)(struct vnode *, const char *);
	int (*create)(struct vnode *, const char *, mode_t, struct vnode **);
	int (*remove)(struct vnode *, const char *);
	int (*symlink)(struct vnode *, const char *, const char *);
	int (*mmap)(struct vnode *, void *, size_t, int, int, size_t);
	int (*getattr)(struct vnode *, struct stat *);
	int (*setattr)(struct vnode *, struct stat *);
};

struct vnode {
	char *path;
	enum vnode_type vtype;
	void *node_data;

	uid_t uid;
	gid_t gid;
	mode_t mode;

	struct vnode_ops *ops;

	struct vfs *vfs_here;
	struct vfs *root_vfs;

	uint32_t refcount;
	spinlock_t vnode_lock;
};

struct vfs {
	struct vfs_fstype fs_type;
	struct vnode *root_vnode;
	void *vfs_data;

	spinlock_t vfs_lock;

	struct vfs_ops *ops;
	struct vfs *next;
};

extern struct vfs *vfs_list;

int vfs_register_fstype(struct vfs_fstype *fstype);
int vfs_unregister_fstype(const char *name);
struct vfs_fstype *vfs_find_fstype(const char *name);

struct vfs *vfs_create_fs(struct vfs_fstype *fs_type, void *fs_data);
struct vfs *vfs_mount(void *device, const char *fstype_name, char *path,
					  void *mount_data);
int vfs_unmount(const char *path);
int vfs_append(struct vfs *vfs);

struct vnode *vnode_create(struct vfs *root_vfs, char *path,
						   enum vnode_type type, void *data);
void vnode_ref(struct vnode *vnode);
void vnode_unref(struct vnode *vnode);

int vfs_resolve_mount(const char *path, struct vfs **out,
					  char **remaining_path);
int vfs_lookup(const char *path, struct vnode **out);
int vfs_lookup_parent(const char *path, struct vnode **parent, char **filename);

int vfs_open(const char *path, int flags, struct fileio **out);
int vfs_read(struct vnode *vnode, size_t size, size_t offset, void *out);
int vfs_write(struct vnode *vnode, void *buf, size_t size, size_t offset);
int vfs_ioctl(struct vnode *vnode, int request, void *arg);
int vfs_close(struct vnode *vnode, int flags, bool clone);

int vfs_readdir(struct vnode *vnode, struct dirent *entries, size_t *count);
int vfs_mkdir(const char *path, int mode);
int vfs_create(const char *path, mode_t mode);
int vfs_rmdir(const char *path);
int vfs_remove(const char *path);

int vfs_readlink(const char *path, char *buf, size_t size);
int vfs_symlink(const char *target, const char *linkpath);

int vfs_stat(const char *path, struct stat *st);
int vfs_setstat(const char *path, struct stat *st);

#endif /* _VFS_VFS_H */
