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
static struct vfs_fstype *registered_fstypes = NULL;
static uint16_t next_fstype_id = 1;

#define MAX_SYMLINK_DEPTH 8

int vfs_register_fstype(struct vfs_fstype *fstype)
{
	if (!fstype || !fstype->name[0] || !fstype->mount) {
		return -1;
	}

	struct vfs_fstype *current = registered_fstypes;
	while (current) {
		if (strcmp(current->name, fstype->name) == 0) {
			return -1;
		}
		current = current->next;
	}

	fstype->id = next_fstype_id++;
	fstype->next = registered_fstypes;
	registered_fstypes = fstype;
	return 0;
}

int vfs_unregister_fstype(const char *name)
{
	if (!name)
		return -1;

	struct vfs_fstype **prev = &registered_fstypes;
	struct vfs_fstype *current = registered_fstypes;

	while (current) {
		if (strcmp(current->name, name) == 0) {
			*prev = current->next;
			return 0;
		}
		prev = &current->next;
		current = current->next;
	}

	return -1;
}

struct vfs_fstype *vfs_find_fstype(const char *name)
{
	if (!name)
		return NULL;

	struct vfs_fstype *current = registered_fstypes;
	while (current) {
		if (strcmp(current->name, name) == 0) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

struct vfs *vfs_create_fs(struct vfs_fstype *fs_type, void *fs_data)
{
	if (!fs_type)
		return NULL;

	struct vfs *vfs = kmalloc(sizeof(struct vfs));
	if (!vfs)
		return NULL;

	memset(vfs, 0, sizeof(struct vfs));
	vfs->fs_type = *fs_type;
	vfs->vfs_data = fs_data;
	vfs->ops = kmalloc(sizeof(struct vfs_ops));

	if (!vfs->ops) {
		kfree(vfs);
		return NULL;
	}

	memset(vfs->ops, 0, sizeof(struct vfs_ops));
	return vfs;
}

struct vfs *vfs_mount(void *device, const char *fstype_name, char *path,
					  void *mount_data)
{
	if (!fstype_name || !path) {
		return NULL;
	}

	struct vfs_fstype *fstype = vfs_find_fstype(fstype_name);
	if (!fstype) {
		return NULL;
	}

	struct vfs *vfs = NULL;
	int ret = fstype->mount(device, path, mount_data, &vfs);

	if (ret != 0 || !vfs) {
		return NULL;
	}

	vfs->fs_type = *fstype;

	struct vnode *mount_point = NULL;
	if (strcmp(path, "/") != 0) {
		ret = vfs_lookup(path, &mount_point);
		if (ret != 0) {
			if (!vfs_list) {
				vfs_append(vfs);
				return vfs;
			}

			if (vfs->ops && vfs->ops->unmount) {
				vfs->ops->unmount(vfs);
			}
			if (vfs->root_vnode) {
				vnode_unref(vfs->root_vnode);
			}
			if (vfs->ops)
				kfree(vfs->ops);
			kfree(vfs);
			return NULL;
		}

		if (mount_point->vtype != VNODE_DIR) {
			vnode_unref(mount_point);
			if (vfs->ops && vfs->ops->unmount) {
				vfs->ops->unmount(vfs);
			}
			if (vfs->root_vnode) {
				vnode_unref(vfs->root_vnode);
			}
			if (vfs->ops)
				kfree(vfs->ops);
			kfree(vfs);
			return NULL;
		}

		mount_point->vfs_here = vfs;
		vnode_unref(mount_point);
	}

	vfs_append(vfs);

	return vfs;
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

int vfs_unmount(const char *path)
{
	if (!path)
		return -1;

	struct vfs **prev = &vfs_list;
	struct vfs *current = vfs_list;

	while (current) {
		if (current->root_vnode &&
			strcmp(current->root_vnode->path, path) == 0) {
			if (strcmp(path, "/") != 0) {
				struct vnode *mount_point;
				if (vfs_lookup(path, &mount_point) == 0) {
					mount_point->vfs_here = NULL;
					vnode_unref(mount_point);
				}
			}

			if (current->ops && current->ops->unmount) {
				current->ops->unmount(current);
			}

			*prev = current->next;

			if (current->root_vnode) {
				vnode_unref(current->root_vnode);
			}

			if (current->ops)
				kfree(current->ops);
			kfree(current);
			return 0;
		}
		prev = &current->next;
		current = current->next;
	}

	return -1;
}

struct vnode *vnode_create(struct vfs *root_vfs, char *path,
						   enum vnode_type type, void *data)
{
	struct vnode *vnode = kmalloc(sizeof(struct vnode));
	if (!vnode)
		return NULL;

	memset(vnode, 0, sizeof(struct vnode));
	vnode->path = strdup(path);
	vnode->vtype = type;
	vnode->root_vfs = root_vfs;
	vnode->node_data = data;
	vnode->refcount = 1;
	vnode->vfs_here = NULL;

	vnode->ops = kmalloc(sizeof(struct vnode_ops));
	if (!vnode->ops) {
		kfree(vnode->path);
		kfree(vnode);
		return NULL;
	}
	memset(vnode->ops, 0, sizeof(struct vnode_ops));

	return vnode;
}

void vnode_ref(struct vnode *vnode)
{
	if (!vnode)
		return;

	spinlock_acquire(&vnode->vnode_lock);
	vnode->refcount++;
	spinlock_release(&vnode->vnode_lock);
}

void vnode_unref(struct vnode *vnode)
{
	if (!vnode)
		return;

	vnode->refcount--;

	if (vnode->refcount == 0) {
		if (vnode->path)
			kfree(vnode->path);
		if (vnode->ops)
			kfree(vnode->ops);
		kfree(vnode);
	}
}

int vfs_resolve_mount(const char *path, struct vfs **out, char **remaining_path)
{
	struct vfs *best_match = NULL;
	size_t best_len = 0;

	for (struct vfs *v = vfs_list; v != NULL; v = v->next) {
		if (!v->root_vnode || !v->root_vnode->path) {
			continue;
		}

		const char *prefix = v->root_vnode->path;
		size_t prefix_len = strlen(prefix);

		if (strlen(path) < prefix_len)
			continue;

		if (prefix_len == 1 && prefix[0] == '/') {
			if (path[0] == '/') {
				if (prefix_len > best_len) {
					best_match = v;
					best_len = prefix_len;
				}
			}
		} else if (strncmp(path, prefix, prefix_len) == 0) {
			if (path[prefix_len] == '\0' || path[prefix_len] == '/') {
				if (prefix_len > best_len) {
					best_match = v;
					best_len = prefix_len;
				}
			}
		}
	}

	if (!best_match) {
		return -1;
	}

	*out = best_match;

	if (remaining_path) {
		const char *rem = path + best_len;
		while (*rem == '/')
			rem++;
		*remaining_path = strdup(rem);
	}

	return 0;
}

static int vfs_lookup_internal(const char *path, struct vnode **out, int depth,
							   bool follow_symlinks);

static char *vfs_normalize_path(const char *path)
{
	if (!path)
		return NULL;

	size_t len = strlen(path);
	bool abs = (len > 0 && path[0] == '/');

	char *temp = kmalloc(len + 1);
	if (!temp)
		return NULL;
	memcpy(temp, path, len + 1);

	size_t max_parts = (len / 2) + 2;
	char **parts = kmalloc(max_parts * sizeof(char *));
	if (!parts) {
		kfree(temp);
		return NULL;
	}

	size_t count = 0;
	char *save = NULL;
	char *token = strtok_r(temp, "/", &save);
	while (token) {
		if (strcmp(token, ".") == 0 || *token == '\0') {
			// skip
		} else if (strcmp(token, "..") == 0) {
			if (count > 0 && strcmp(parts[count - 1], "..") != 0) {
				count--;
			} else if (!abs) {
				parts[count++] = token;
			}
		} else {
			parts[count++] = token;
		}
		token = strtok_r(NULL, "/", &save);
	}

	size_t out_len = abs ? 1 : 0;
	for (size_t i = 0; i < count; i++)
		out_len += strlen(parts[i]) + 1;
	if (out_len == 0)
		out_len = 1;

	char *out = kmalloc(out_len + 1);
	if (!out) {
		kfree(parts);
		kfree(temp);
		return NULL;
	}

	char *cursor = out;
	if (abs)
		*cursor++ = '/';

	for (size_t i = 0; i < count; i++) {
		size_t part_len = strlen(parts[i]);
		memcpy(cursor, parts[i], part_len);
		cursor += part_len;
		if (i + 1 < count)
			*cursor++ = '/';
	}

	if (cursor == out) {
		*cursor++ = '/';
	}
	*cursor = '\0';

	kfree(parts);
	kfree(temp);
	return out;
}

static int vfs_follow_symlink(struct vnode *vnode, struct vnode **out,
							  int depth)
{
	if (depth >= MAX_SYMLINK_DEPTH) {
		return -1;
	}

	if (vnode->vtype != VNODE_LINK) {
		*out = vnode;
		return 0;
	}

	if (!vnode->ops || !vnode->ops->readlink) {
		return -1;
	}

	char target[512];
	int ret = vnode->ops->readlink(vnode, target, sizeof(target));
	if (ret != 0) {
		return ret;
	}

	const char *lookup_path = target;
	char *resolved_path = NULL;
	if (target[0] != '/') {
		const char *link_path = vnode->path ? vnode->path : "/";
		const char *last_slash = strrchr(link_path, '/');
		size_t parent_len = last_slash ? (size_t)(last_slash - link_path) : 0;
		size_t target_len = strlen(target);
		size_t total_len = parent_len + 1 + target_len + 1;

		resolved_path = kmalloc(total_len);
		if (!resolved_path)
			return -1;

		if (parent_len == 0) {
			strcpy(resolved_path, "/");
		} else {
			memcpy(resolved_path, link_path, parent_len);
			resolved_path[parent_len] = '\0';
		}
		if (resolved_path[strlen(resolved_path) - 1] != '/')
			strcat(resolved_path, "/");
		strcat(resolved_path, target);
		lookup_path = resolved_path;
	}

	char *normalized = vfs_normalize_path(lookup_path);
	if (normalized)
		lookup_path = normalized;

	// Resolve the symlink target
	struct vnode *target_vnode;
	ret = vfs_lookup_internal(lookup_path, &target_vnode, depth + 1, false);
	if (resolved_path)
		kfree(resolved_path);
	if (normalized)
		kfree(normalized);
	if (ret != 0) {
		return ret;
	}

	*out = target_vnode;
	return 0;
}

static int vfs_lookup_internal(const char *path, struct vnode **out, int depth,
							   bool follow_symlinks)
{
	if (depth >= MAX_SYMLINK_DEPTH) {
		return -1;
	}

	struct vfs *vfs;
	char *rel_path;
	int ret = vfs_resolve_mount(path, &vfs, &rel_path);
	if (ret != 0) {
		return ret;
	}

	if (!rel_path || rel_path[0] == '\0') {
		if (rel_path)
			kfree(rel_path);

		if (vfs->root_vnode && vfs->root_vnode->vfs_here) {
			*out = vfs->root_vnode->vfs_here->root_vnode;
		} else {
			*out = vfs->root_vnode;
		}
		vnode_ref(*out);
		return 0;
	}

	struct vnode *current = vfs->root_vnode;
	if (!current) {
		kfree(rel_path);
		return -1;
	}

	vnode_ref(current);

	if (current->vfs_here) {
		vfs = current->vfs_here;
		vnode_unref(current);
		current = vfs->root_vnode;
		vnode_ref(current);
	}

	char *path_copy = rel_path;
	char *component = strtok(path_copy, "/");

	while (component) {
		if (current->vtype != VNODE_DIR) {
			vnode_unref(current);
			kfree(rel_path);
			return -1;
		}

		// TODO: perms

		if (!current->ops || !current->ops->lookup) {
			vnode_unref(current);
			kfree(rel_path);
			return -1;
		}

		struct vnode *next;
		ret = current->ops->lookup(current, component, &next);
		if (ret != 0) {
			vnode_unref(current);
			kfree(rel_path);
			return ret;
		}

		if (next->vfs_here) {
			vnode_unref(next);
			next = next->vfs_here->root_vnode;
			vnode_ref(next);
		}

		struct vnode *resolved = next;
		if (follow_symlinks) {
			ret = vfs_follow_symlink(next, &resolved, depth + 1);
			if (ret != 0) {
				vnode_unref(next);
				vnode_unref(current);
				kfree(rel_path);
				return ret;
			}
		}

		if (resolved != next) {
			vnode_unref(next);
			vnode_ref(resolved);
		}

		vnode_unref(current);
		current = resolved;

		component = strtok(NULL, "/");
	}

	kfree(rel_path);
	*out = current;
	return 0;
}

int vfs_lookup(const char *path, struct vnode **out)
{
	return vfs_lookup_internal(path, out, 0, true);
}

int vfs_lookup_parent(const char *path, struct vnode **parent, char **filename)
{
	if (!path || !parent || !filename)
		return -1;

	const char *last_slash = strrchr(path, '/');
	if (!last_slash) {
		return -1;
	}

	size_t parent_len = last_slash - path;

	char *parent_path;
	if (parent_len == 0) {
		parent_path = strdup("/");
	} else {
		parent_path = kmalloc(parent_len + 1);
		if (!parent_path)
			return -1;
		memcpy(parent_path, path, parent_len);
		parent_path[parent_len] = '\0';
	}

	*filename = strdup(last_slash + 1);
	if (!*filename) {
		kfree(parent_path);
		return -1;
	}

	int ret = vfs_lookup(parent_path, parent);
	kfree(parent_path);

	if (ret != 0) {
		kfree(*filename);
		*filename = NULL;
		return ret;
	}

	return 0;
}

int vfs_create(const char *path, mode_t mode)
{
	struct vnode *vnode = NULL;

	struct vnode *parent;
	char *fname;

	int ret = vfs_lookup_parent(path, &parent, &fname);
	if (ret != 0)
		return ret;

	if (!parent->ops || !parent->ops->create) {
		vnode_unref(parent);
		kfree(fname);
		return -1;
	}

	ret = parent->ops->create(parent, fname, mode, &vnode);
	kfree(fname);
	vnode_unref(parent);

	if (ret != 0)
		return ret;

	return 0;
}

int vfs_open(const char *path, int flags, struct fileio **out)
{
	if (!path || !out)
		return -1;

	struct vnode *vnode = NULL;
	int ret = vfs_lookup(path, &vnode);

	// TODO: perms

	if (ret == -1 && (flags & V_CREATE)) {
		struct vnode *parent;
		char *fname;

		ret = vfs_lookup_parent(path, &parent, &fname);
		if (ret != 0)
			return ret;

		if (!parent->ops || !parent->ops->create) {
			vnode_unref(parent);
			kfree(fname);
			return -1;
		}

		ret = parent->ops->create(parent, fname, flags, &vnode);
		kfree(fname);
		vnode_unref(parent);

		if (ret != 0)
			return ret;
	} else if (ret != 0) {
		return ret;
	}

	if (!vnode) {
		return -1;
	}

	if (!vnode->ops || !vnode->ops->open) {
		vnode_unref(vnode);
		return -1;
	}

	struct fileio *fio = fio_create();
	if (!fio) {
		vnode_unref(vnode);
		return -1;
	}

	ret = vnode->ops->open(&vnode, flags, false, &fio);
	if (ret != 0) {
		kfree(fio);
		vnode_unref(vnode);
		return ret;
	}

	fio->private = vnode;
	*out = fio;

	return 0;
}

int vfs_read(struct vnode *vnode, size_t size, size_t offset, void *out)
{
	if (!vnode) {
		return -1;
	}

	int ret = vnode->ops->read(vnode, &size, &offset, out);

	return ret;
}

int vfs_write(struct vnode *vnode, void *buf, size_t size, size_t offset)
{
	if (!vnode) {
		return -1;
	}

	int ret = vnode->ops->write(vnode, buf, &size, &offset);

	if (ret != 0) {
		return ret;
	} else {
		return size;
	}
}

int vfs_ioctl(struct vnode *vnode, int request, void *arg)
{
	if (!vnode) {
		return -1;
	}

	int ret = vnode->ops->ioctl(vnode, request, arg);

	return ret;
}

int vfs_close(struct vnode *vnode)
{
	if (!vnode) {
		return -1;
	}

	if (!vnode->ops || !vnode->ops->close) {
		vnode_unref(vnode);
		return -1;
	}

	int r = vnode->ops->close(vnode, 0, false);

	if (r != 0) {
		vnode_unref(vnode);
		return r;
	}

	vnode_unref(vnode);

	return 0;
}

int vfs_readdir(struct vnode *vnode, struct dirent *entries, size_t *count)
{
	if (!vnode || !entries || !count)
		return -1;

	if (vnode->vtype != VNODE_DIR) {
		return -1;
	}

	if (!vnode->ops || !vnode->ops->readdir) {
		return -1;
	}

	return vnode->ops->readdir(vnode, entries, count);
}

int vfs_mkdir(const char *path, int mode)
{
	if (!path)
		return -1;

	struct vnode *parent;
	char *dirname;

	int ret = vfs_lookup_parent(path, &parent, &dirname);
	if (ret != 0)
		return ret;

	if (!parent->ops || !parent->ops->mkdir) {
		vnode_unref(parent);
		kfree(dirname);
		return -1;
	}

	// TODO: perms

	ret = parent->ops->mkdir(parent, dirname, mode);
	kfree(dirname);
	vnode_unref(parent);

	return ret;
}

int vfs_rmdir(const char *path)
{
	if (!path)
		return -1;

	struct vnode *parent;
	char *dirname;

	int ret = vfs_lookup_parent(path, &parent, &dirname);
	if (ret != 0)
		return ret;

	if (!parent->ops || !parent->ops->rmdir) {
		vnode_unref(parent);
		kfree(dirname);
		return -1;
	}

	// TODO: perms

	ret = parent->ops->rmdir(parent, dirname);
	kfree(dirname);
	vnode_unref(parent);

	return ret;
}

int vfs_remove(const char *path)
{
	if (!path)
		return -1;

	struct vnode *parent;
	char *filename;

	int ret = vfs_lookup_parent(path, &parent, &filename);
	if (ret != 0)
		return ret;

	if (!parent->ops || !parent->ops->remove) {
		vnode_unref(parent);
		kfree(filename);
		return -1;
	}

	// TODO: perms

	ret = parent->ops->remove(parent, filename);
	kfree(filename);
	vnode_unref(parent);

	return ret;
}

int vfs_readlink(const char *path, char *buf, size_t size)
{
	if (!path || !buf)
		return -1;

	struct vnode *vnode;
	int ret = vfs_lookup_internal(path, &vnode, 0, false);
	if (ret != 0)
		return ret;

	if (vnode->vtype != VNODE_LINK) {
		vnode_unref(vnode);
		return -1;
	}

	if (!vnode->ops || !vnode->ops->readlink) {
		vnode_unref(vnode);
		return -1;
	}

	ret = vnode->ops->readlink(vnode, buf, size);
	vnode_unref(vnode);

	return ret;
}

int vfs_symlink(const char *target, const char *linkpath)
{
	if (!target || !linkpath)
		return -1;

	struct vnode *parent;
	char *linkname;

	int ret = vfs_lookup_parent(linkpath, &parent, &linkname);
	if (ret != 0)
		return ret;

	if (!parent->ops || !parent->ops->symlink) {
		vnode_unref(parent);
		kfree(linkname);
		return -1;
	}

	ret = parent->ops->symlink(parent, linkname, target);
	kfree(linkname);
	vnode_unref(parent);

	return ret;
}

int vfs_stat(const char *path, struct stat *st)
{
	if (!path || !st)
		return -1;

	struct vnode *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	if (!vnode->ops || !vnode->ops->getattr) {
		vnode_unref(vnode);
		return -1;
	}

	ret = vnode->ops->getattr(vnode, st);
	vnode_unref(vnode);

	return ret;
}

int vfs_setstat(const char *path, struct stat *st)
{
	if (!path || !st)
		return -1;

	struct vnode *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	if (!vnode->ops || !vnode->ops->setattr) {
		vnode_unref(vnode);
		return -1;
	}

	ret = vnode->ops->setattr(vnode, st);
	vnode_unref(vnode);

	return ret;
}
