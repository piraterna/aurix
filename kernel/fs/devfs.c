/*********************************************************************************/
/* Module Name:  devfs.c */
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

#include <fs/devfs.h>
#include <dev/device.h>
#include <mm/heap.h>
#include <lib/string.h>
#include <debug/log.h>
#include <debug/assert.h>
#include <aurix.h>
#include <user/access.h>

struct devfs *devfs_create_fs()
{
	struct devfs *devfs = kmalloc(sizeof(struct devfs));
	if (!devfs) {
		return NULL;
	}

	memset(devfs, 0, sizeof(struct devfs));
	return devfs;
}

struct devfs_node *devfs_create_fs_node(devfs_ftype_t ftype)
{
	struct devfs_node *node = kmalloc(sizeof(struct devfs_node));
	if (!node) {
		return NULL;
	}

	memset(node, 0, sizeof(struct devfs_node));
	node->type = ftype;

	switch (ftype) {
	case DEVFS_TYPE_DIR:
		node->mode = S_IFDIR | 0755;
		break;
	case DEVFS_TYPE_CHAR:
		node->mode = S_IFCHR | 0666;
		break;
	case DEVFS_TYPE_BLOCK:
		node->mode = S_IFBLK | 0666;
		break;
	case DEVFS_TYPE_FILE:
		node->mode = S_IFREG | 0644;
		break;
	}

	return node;
}

int devfs_find_node(struct devfs *devfs, char *path, struct devfs_node **out)
{
	if (!devfs || !devfs->root_node || !path || !out) {
		return -1;
	}

	*out = NULL;

	if (path[0] == '/')
		path++;

	if (*path == '\0') {
		*out = devfs->root_node;
		return 0;
	}

	char *dup = strdup(path);
	if (!dup)
		return -1;

	char *save = NULL;
	char *token = strtok_r(dup, "/", &save);

	struct devfs_node *cur = devfs->root_node;

	while (token && cur) {
		struct devfs_node *child;
		for (child = cur->child; child; child = child->sibling) {
			if (strcmp(child->name, token) == 0) {
				break;
			}
		}

		if (!child) {
			kfree(dup);
			return -1;
		}

		cur = child;
		token = strtok_r(NULL, "/", &save);
	}

	kfree(dup);
	*out = cur;
	return 0;
}

int devfs_node_add(struct devfs *devfs, char *path, struct devfs_node **out)
{
	if (!devfs || !devfs->root_node || !path || !out) {
		return -1;
	}

	if (path[0] == '/')
		path++;

	char *dup = strdup(path);
	if (!dup)
		return -1;

	char *save = NULL;
	char *token = strtok_r(dup, "/", &save);

	struct devfs_node *cur = devfs->root_node;

	while (token) {
		struct devfs_node *child;
		for (child = cur->child; child; child = child->sibling) {
			if (strcmp(child->name, token) == 0) {
				break;
			}
		}

		if (!child) {
			devfs_ftype_t type =
				strtok_r(NULL, "/", &save) ? DEVFS_TYPE_DIR : DEVFS_TYPE_FILE;

			struct devfs_node *n = devfs_create_fs_node(type);
			n->name = strdup(token);

			for (int i = 0; i < device_count; i++) {
				if (device_list[i] &&
					strcmp(device_list[i]->dev_node_path, token) == 0) {
					n->device = device_list[i];
					break;
				}
			}

			devfs_append_child(cur, n);
			child = n;
		}

		cur = child;
		token = strtok_r(NULL, "/", &save);
	}

	kfree(dup);
	*out = cur;
	return 0;
}

int devfs_append_child(struct devfs_node *parent, struct devfs_node *child)
{
	if (!parent || !child) {
		warn("devfs_append_child: invalid parent or child\n");
		return -1;
	}

	if (!parent->child) {
		parent->child = child;
		return 0;
	}

	struct devfs_node *last_child = parent->child;
	while (last_child->sibling)
		last_child = last_child->sibling;

	last_child->sibling = child;
	return 0;
}

int devfs_find_or_create_node(struct devfs *devfs, char *path,
							  devfs_ftype_t devfs_ftype,
							  struct devfs_node **out)
{
	struct devfs_node *found = NULL;
	devfs_find_node(devfs, path, &found);

	if (!found) {
		*out = devfs_create_fs_node(devfs_ftype);
		return 1;
	}

	*out = found;
	return 0;
}

int devfs_print(struct devfs_node *devfs, int lvl)
{
	if (!devfs) {
		return -1;
	}

	int a = lvl;

	while (a--) {
		kprintf("  ");
	}

	switch (devfs->type) {
	case DEVFS_TYPE_DIR:
		kprintf("+ %-20s", devfs->name);
		break;

	case DEVFS_TYPE_FILE:
		kprintf("  %-20s", devfs->name);
		kprintf(" | % 10zuB", devfs->device ? 0 : 0);
		break;

	case DEVFS_TYPE_CHAR:
	case DEVFS_TYPE_BLOCK:
		kprintf("  %-20s", devfs->name);
		break;
	}

	kprintf("\n");

	if (devfs->child) {
		devfs_print(devfs->child, lvl + 1);
	}

	if (devfs->sibling) {
		devfs_print(devfs->sibling, lvl);
	}

	return 0;
}

int devfs_refresh(void)
{
	for (struct vfs *vfs = vfs_list; vfs != NULL; vfs = vfs->next) {
		if (strcmp(vfs->fs_type.name, "devfs") != 0) {
			continue;
		}

		struct devfs *devfs = (struct devfs *)vfs->vfs_data;
		if (!devfs || !devfs->root_node) {
			continue;
		}

		struct devfs_node *root = devfs->root_node;

		struct devfs_node *prev = NULL;
		struct devfs_node *cur = root->child;

		while (cur) {
			struct devfs_node *next = cur->sibling;
			bool still_exists = false;

			for (int i = 0; i < device_count; i++) {
				if (!device_list[i]) {
					continue;
				}

				if (strcmp(device_list[i]->dev_node_path, cur->name) == 0) {
					still_exists = true;
					break;
				}
			}

			if (!still_exists) {
				if (prev) {
					prev->sibling = next;
				} else {
					root->child = next;
				}

				if (cur->name) {
					kfree(cur->name);
				}
				kfree(cur);

				cur = next;
				continue;
			}

			prev = cur;
			cur = next;
		}

		for (int i = 0; i < device_count; i++) {
			struct device *dev = device_list[i];
			if (!dev) {
				continue;
			}

			bool found = false;

			struct devfs_node *node = root->child;
			while (node) {
				if (strcmp(node->name, dev->dev_node_path) == 0) {
					found = true;
					break;
				}
				node = node->sibling;
			}

			if (!found) {
				// TODO: block dev
				struct devfs_node *new_node =
					devfs_create_fs_node(DEVFS_TYPE_CHAR);
				if (!new_node) {
					return -1;
				}

				new_node->name = strdup(dev->dev_node_path);
				new_node->device = dev;

				if (!new_node->name) {
					kfree(new_node);
					return -1;
				}

				devfs_append_child(root, new_node);
			}
		}
	}

	return 0;
}

int devfs_open(struct vnode **vnode_r, int flags, bool clone,
			   struct fileio **fio_out)
{
	(void)(flags);
	(void)(clone);

	if (!vnode_r || !*vnode_r || !fio_out || !*fio_out)
		return -1;

	struct vnode *vnode = *vnode_r;
	struct devfs *devfs = vnode->root_vfs->vfs_data;
	if (!devfs)
		return -1;

	struct devfs_node *devfs_node = NULL;

	char *rel_path = vnode->path + strlen(vnode->root_vfs->root_vnode->path);
	if (rel_path[0] == '/')
		rel_path++;

	int find_res = devfs_find_node(devfs, rel_path, &devfs_node);
	if (find_res != 0) {
		if (!(flags & V_CREATE))
			return -1;

		if (devfs_node_add(devfs, rel_path, &devfs_node) != 0)
			return -1;
	}

	vnode->node_data = devfs_node;

	struct fileio *fio_file = *fio_out;
	fio_file->buf_start = NULL;
	fio_file->flags |= SPECIAL_FILE_TYPE_DEVICE;
	fio_file->size = 0;

	return 0;
}

int devfs_close(struct vnode *vnode, int flags, bool clone)
{
	(void)(flags);
	(void)(clone);
	if (!vnode) {
		return -1;
	}

	return 0;
}

int devfs_ioctl(struct vnode *vnode, int request, void *arg)
{
	if (!vnode) {
		return -1;
	}

	struct device *dev = vnode->node_data ?
							 ((struct devfs_node *)vnode->node_data)->device :
							 NULL;

	if (!dev || !dev->ops->ioctl) {
		return -1;
	}

	return dev->ops->ioctl(dev, request, arg);
}

int devfs_read(struct vnode *vn, size_t *bytes, size_t *offset, void *out)
{
	if (!vn || !bytes || !offset || !out) {
		return -1;
	}

	struct devfs_node *node = vn->node_data;
	if (!node || !node->device || !node->device->ops->read) {
		return -1;
	}

	int ret = node->device->ops->read(node->device, out, *bytes, *offset);

	if (ret < 0) {
		return ret;
	}

	*bytes = (size_t)ret;
	*offset += (size_t)ret;
	return 0;
}

int devfs_write(struct vnode *vn, void *buf, size_t *bytes, size_t *offset)
{
	if (!vn || !buf || !bytes || !offset) {
		return -1;
	}

	struct devfs_node *node = vn->node_data;
	if (!node || !node->device || !node->device->ops->write) {
		return -1;
	}

	int ret = node->device->ops->write(node->device, buf, *bytes, *offset);

	if (ret < 0) {
		return ret;
	}

	*bytes = (size_t)ret;
	*offset += (size_t)ret;
	return 0;
}

int devfs_lookup(struct vnode *parent, const char *name, struct vnode **out)
{
	if (!parent || !name || !out)
		return -1;

	struct devfs_node *pnode = parent->node_data;
	if (!pnode)
		return -1;

	if (pnode->type != DEVFS_TYPE_DIR)
		return -1;

	for (struct devfs_node *c = pnode->child; c; c = c->sibling) {
		if (strcmp(c->name, name) == 0) {
			size_t plen = strlen(parent->path);
			size_t nlen = strlen(name);

			char *path = kmalloc(plen + nlen + 2);
			if (!path)
				return -1;

			strcpy(path, parent->path);
			if (path[plen - 1] != '/')
				strcat(path, "/");
			strcat(path, name);

			enum vnode_type t = c->type == DEVFS_TYPE_DIR	? VNODE_DIR :
								c->type == DEVFS_TYPE_BLOCK ? VNODE_BLOCK :
								c->type == DEVFS_TYPE_CHAR	? VNODE_CHAR :
															  VNODE_REGULAR;

			struct vnode *vn = vnode_create(parent->root_vfs, path, t, c);
			memcpy(vn->ops, parent->ops, sizeof(struct vnode_ops));
			vn->mode = c->mode;

			*out = vn;
			return 0;
		}
	}

	return -1;
}

int devfs_readdir(struct vnode *vnode, struct dirent *entries, size_t *count)
{
	if (!vnode || !entries || !count)
		return -1;
	if (vnode->vtype != VNODE_DIR)
		return -1;

	struct devfs_node *dir = vnode->node_data;
	if (!dir || dir->type != DEVFS_TYPE_DIR)
		return -1;

	size_t i = 0;
	for (struct devfs_node *c = dir->child; c && i < *count; c = c->sibling) {
		entries[i].d_ino = (uint64_t)c;
		entries[i].d_off = i + 1;
		entries[i].d_reclen = sizeof(struct dirent);
		entries[i].d_type = c->type == DEVFS_TYPE_DIR	? DT_DIR :
							c->type == DEVFS_TYPE_BLOCK ? DT_BLK :
							c->type == DEVFS_TYPE_CHAR	? DT_CHR :
														  DT_REG;
		strncpy(entries[i].d_name, c->name, sizeof(entries[i].d_name) - 1);
		entries[i].d_name[sizeof(entries[i].d_name) - 1] = 0;
		i++;
	}

	*count = i;
	return 0;
}

static int devfs_mmap(struct vnode *vnode, void *addr, size_t length, int prot,
					  int flags, size_t offset)
{
	(void)vnode;
	(void)addr;
	(void)length;
	(void)prot;
	(void)flags;
	(void)offset;
	// not implemented
	return -1;
}

static int devfs_getattr(struct vnode *vnode, struct stat *st)
{
	if (!vnode || !st) {
		return -1;
	}

	struct devfs_node *node = vnode->node_data;
	if (!node) {
		return -1;
	}

	st->st_dev = 0;
	st->st_ino = (uint64_t)node;
	st->st_nlink = 1;
	st->st_mode = node->mode;
	st->st_uid = 0;
	st->st_gid = 0;

	if (node->device) {
		st->st_size = 0;
		st->st_blksize = 512;

		if (node->type == DEVFS_TYPE_BLOCK) {
			st->st_mode |= S_IFBLK;
		} else if (node->type == DEVFS_TYPE_CHAR) {
			st->st_mode |= S_IFCHR;
		} else {
			st->st_mode |= S_IFREG;
		}
	} else {
		st->st_size = 0;
		st->st_blksize = 512;
		st->st_mode |= S_IFDIR;
	}

	return 0;
}

static int devfs_setattr(struct vnode *vnode, struct stat *st)
{
	if (!vnode || !st) {
		return -1;
	}

	struct devfs_node *node = vnode->node_data;
	if (!node) {
		return -1;
	}

	node->mode = st->st_mode;
	vnode->uid = st->st_uid;
	vnode->gid = st->st_gid;

	return 0;
}

struct vnode_ops devfs_vnops = { .open = devfs_open,
								 .close = devfs_close,
								 .read = devfs_read,
								 .write = devfs_write,
								 .ioctl = devfs_ioctl,
								 .lookup = devfs_lookup,
								 .readdir = devfs_readdir,
								 .mmap = devfs_mmap,
								 .getattr = devfs_getattr,
								 .setattr = devfs_setattr };

static int devfs_vfs_mount(struct vfs *vfs, char *path, void *data)
{
	(void)(path);
	(void)(data);

	if (!vfs) {
		return -1;
	}

	return 0;
}

static int devfs_vfs_unmount(struct vfs *vfs)
{
	if (!vfs) {
		return -1;
	}

	struct devfs *devfs = vfs->vfs_data;
	if (devfs && devfs->root_node) {
		// TODO: free devfs tree
	}

	return 0;
}

static int devfs_vfs_root(struct vfs *vfs, struct vnode **out)
{
	if (!vfs || !out) {
		return -1;
	}

	*out = vfs->root_vnode;
	vnode_ref(*out);

	return 0;
}

static int devfs_vfs_statfs(struct vfs *vfs, struct statfs *stat)
{
	if (!vfs || !stat) {
		return -1;
	}

	struct devfs *devfs = vfs->vfs_data;
	if (!devfs) {
		return -1;
	}

	stat->block_size = 1;
	stat->total_blocks = devfs->devfs_size;
	stat->free_blocks = 0;
	stat->total_nodes = device_count;
	stat->free_nodes = 0;

	return 0;
}

static int devfs_vfs_sync(struct vfs *vfs)
{
	(void)(vfs);
	return 0;
}

struct vfs_ops devfs_vfsops = {
	.mount = devfs_vfs_mount,
	.unmount = devfs_vfs_unmount,
	.root = devfs_vfs_root,
	.statfs = devfs_vfs_statfs,
	.sync = devfs_vfs_sync,
};

static int devfs_fstype_mount(void *device, char *mount_point, void *mount_data,
							  struct vfs **out)
{
	(void)(mount_data);

	struct devfs *devfs = (struct devfs *)device;
	if (!devfs) {
		devfs = devfs_create_fs();
		if (!devfs) {
			return -1;
		}
	}

	struct vfs_fstype fstype;
	memset(&fstype, 0, sizeof(struct vfs_fstype));
	strncpy(fstype.name, "devfs", sizeof(fstype.name) - 1);

	struct vfs *vfs = vfs_create_fs(&fstype, devfs);
	if (!vfs) {
		return -1;
	}

	memcpy(vfs->ops, &devfs_vfsops, sizeof(struct vfs_ops));

	struct devfs_node *root_node = devfs_create_fs_node(DEVFS_TYPE_DIR);
	if (!root_node) {
		kfree(vfs->ops);
		kfree(vfs);
		return -1;
	}

	root_node->name = strdup("/");
	if (!root_node->name) {
		kfree(root_node);
		kfree(vfs->ops);
		kfree(vfs);
		return -1;
	}

	devfs->root_node = root_node;
	devfs->devfs_size = 0;
	root_node->device = NULL;
	root_node->sibling = NULL;
	root_node->child = NULL;
	root_node->type = DEVFS_TYPE_DIR;

	vfs->root_vnode = vnode_create(vfs, mount_point, VNODE_DIR, root_node);
	if (!vfs->root_vnode) {
		kfree(root_node->name);
		kfree(root_node);
		kfree(vfs->ops);
		kfree(vfs);
		return -1;
	}
	vfs->root_vnode->node_data = root_node;
	vfs->root_vnode->mode = root_node->mode;

	memcpy(vfs->root_vnode->ops, &devfs_vnops, sizeof(struct vnode_ops));

	for (int i = 0; i < device_count; i++) {
		struct device *dev = device_list[i];
		if (!dev) {
			continue;
		}

		// TODO: add block devices
		struct devfs_node *devfs_node = devfs_create_fs_node(DEVFS_TYPE_CHAR);
		devfs_node->name = strdup(dev->dev_node_path);
		devfs_node->device = dev;

		devfs_append_child(devfs->root_node, devfs_node);
	}

	*out = vfs;
	return 0;
}

static struct vfs_fstype devfs_fstype = { .id = 0,
										  .name = "devfs",
										  .mount = devfs_fstype_mount,
										  .next = NULL };

void devfs_init(void)
{
	vfs_register_fstype(&devfs_fstype);
}

int devfs_vfs_init(struct devfs *devfs, char *mount_path)
{
	if (!mount_path) {
		return -1;
	}

	struct vfs *vfs = vfs_mount(devfs, "devfs", mount_path, NULL);
	if (!vfs) {
		return -1;
	}

	return 0;
}
