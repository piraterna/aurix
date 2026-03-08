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

devfs_t *global_devfs = NULL;

devfs_t *devfs_create()
{
	devfs_t *devfs = kmalloc(sizeof(devfs_t));
	if (!devfs) {
		error("devfs_create: kmalloc failed\n");
		return NULL;
	}

	memset(devfs, 0, sizeof(devfs_t));
	return devfs;
}

devfs_node_t *devfs_create_node(devfs_ftype_t ftype)
{
	devfs_node_t *node = kmalloc(sizeof(devfs_node_t));
	if (!node) {
		error("devfs_create_node: kmalloc failed\n");
		return NULL;
	}

	memset(node, 0, sizeof(devfs_node_t));
	node->type = ftype;

	return node;
}

int devfs_find_node(devfs_t *devfs, char *path, devfs_node_t **out)
{
	if (!devfs || !devfs->root_node || !path || !out) {
		error("devfs_find_node: invalid args\n");
		return -1;
	}

	devfs_node_t *cur_node = devfs->root_node->child;
	*out = NULL;

	if (path[0] == '/')
		path++;

	char *dup = strdup(path);
	char *save = NULL;

	char *dir = strtok_r(dup, "/", &save);

	while (dir && cur_node) {
		while (cur_node) {
			if (strcmp(cur_node->name, dir) == 0)
				break;

			cur_node = cur_node->sibling;
		}

		if (!cur_node)
			break;

		dir = strtok_r(NULL, "/", &save);

		if (dir)
			cur_node = cur_node->child;
	}

	kfree(dup);

	*out = cur_node;

	return cur_node ? 0 : -1;
}

int devfs_append_child(devfs_node_t *parent, devfs_node_t *child)
{
	if (!parent || !child) {
		error("devfs_append_child: invalid args\n");
		return -1;
	}

	if (!parent->child) {
		parent->child = child;
		return 0;
	}

	devfs_node_t *n = parent->child;

	while (n->sibling)
		n = n->sibling;

	n->sibling = child;

	return 0;
}

int devfs_node_add(devfs_t *devfs, char *path, devfs_node_t **out)
{
	if (!devfs || !devfs->root_node || !path) {
		error("devfs_node_add: invalid args\n");
		return -1;
	}

	if (path[0] == '/')
		path++;

	char *dup = strdup(path);
	char *save = NULL;

	char *dir = strtok_r(dup, "/", &save);

	devfs_node_t *cur = devfs->root_node;

	while (dir) {
		char *next = strtok_r(NULL, "/", &save);

		devfs_node_t *child = cur->child;
		devfs_node_t *found = NULL;

		while (child) {
			if (strcmp(child->name, dir) == 0) {
				found = child;
				break;
			}
			child = child->sibling;
		}

		if (!found) {
			devfs_ftype_t type = next ? DEVFS_TYPE_DIR : DEVFS_TYPE_FILE;

			found = devfs_create_node(type);
			if (!found) {
				kfree(dup);
				return -1;
			}

			found->name = strdup(dir);

			if (!next) {
				struct device *dev = device_get_list();

				while (dev) {
					if (dev->dev_node_path &&
						strcmp(dev->dev_node_path, dir) == 0) {
						found->device = dev;
						found->type = DEVFS_TYPE_CHAR;
						break;
					}

					dev = dev->next;
				}
			}

			if (devfs_append_child(cur, found) != 0) {
				kfree(dup);
				return -1;
			}
		}

		cur = found;
		dir = next;
	}

	if (out)
		*out = cur;

	kfree(dup);

	return 0;
}

int devfs_refresh(devfs_t *devfs)
{
	if (!devfs || !devfs->root_node) {
		error("devfs_refresh: invalid args\n");
		return -1;
	}

	devfs_node_t *root = devfs->root_node;

	struct device *dev = device_get_list();

	while (dev) {
		bool found = false;

		devfs_node_t *node = root->child;

		while (node) {
			if (strcmp(node->name, dev->dev_node_path) == 0) {
				found = true;
				break;
			}

			node = node->sibling;
		}

		if (!found) {
			devfs_node_t *n = devfs_create_node(DEVFS_TYPE_CHAR);

			if (!n)
				return -1;

			n->name = strdup(dev->dev_node_path);
			n->device = dev;

			devfs_append_child(root, n);
		}

		dev = dev->next;
	}

	return 0;
}

int devfs_open(struct vnode **vnode, int flags, bool create,
			   struct fileio **out)
{
	(void)flags;
	(void)create;
	if (!vnode || !*vnode) {
		error("devfs_open: vnode null\n");
		return -1;
	}

	struct vnode *vn = *vnode;
	devfs_t *devfs = vn->vfs_mount->fs_data;

	devfs_node_t *node = NULL;

	if (devfs_find_node(devfs, vn->path, &node) != 0) {
		error("devfs_open: node not found: %s\n", vn->path);
		return -1;
	}

	vn->node_data = node;

	if (node->type == DEVFS_TYPE_DIR)
		vn->vtype = VNODE_DIR;
	else if (node->type == DEVFS_TYPE_CHAR)
		vn->vtype = VNODE_CHAR;
	else
		vn->vtype = VNODE_REGULAR;

	if (node->device && node->device->ops && node->device->ops->open) {
		if (node->device->ops->open(node->device) != 0) {
			error("devfs_open: device open failed\n");
			return -1;
		}
	}

	if (out && *out) {
		(*out)->private = vn;
	}

	return 0;
}

int devfs_close(struct vnode *vn, int flags, bool create)
{
	(void)flags;
	(void)create;
	if (!vn) {
		error("devfs_close: vnode null\n");
		return -1;
	}

	devfs_node_t *node = vn->node_data;

	if (node && node->device && node->device->ops && node->device->ops->close) {
		return node->device->ops->close(node->device);
	}

	return 0;
}

int devfs_read(struct vnode *vn, size_t *bytes, size_t *offset, void *out)
{
	if (!vn || !bytes || !offset || !out) {
		error("devfs_read: invalid args\n");
		return -1;
	}

	devfs_node_t *node = vn->node_data;

	if (!node || !node->device || !node->device->ops ||
		!node->device->ops->read) {
		error("devfs_read: read unsupported\n");
		return -1;
	}

	return node->device->ops->read(node->device, out, *bytes);
}

int devfs_write(struct vnode *vn, void *buf, size_t *bytes, size_t *offset)
{
	if (!vn || !buf || !bytes || !offset) {
		error("devfs_write: invalid args\n");
		return -1;
	}

	devfs_node_t *node = vn->node_data;

	if (!node || !node->device || !node->device->ops ||
		!node->device->ops->write) {
		error("devfs_write: write unsupported\n");
		return -1;
	}

	return node->device->ops->write(node->device, buf, *bytes);
}

int devfs_ioctl(struct vnode *vnode, int request, void *arg)
{
	if (!vnode) {
		error("devfs_ioctl: vnode null\n");
		return -1;
	}

	devfs_node_t *node = vnode->node_data;

	if (!node || !node->device || !node->device->ops ||
		!node->device->ops->ioctl) {
		error("devfs_ioctl: ioctl unsupported\n");
		return -1;
	}

	return node->device->ops->ioctl(node->device, request, arg);
}

struct vfs_vops devfs_vnops = {
	.open = devfs_open,
	.close = devfs_close,

	.read = devfs_read,
	.write = devfs_write,
	.ioctl = devfs_ioctl,
};

int devfs_vfs_init(devfs_t *devfs, char *mount_path)
{
	if (!devfs || !mount_path) {
		error("devfs_vfs_init: invalid args\n");
		return -1;
	}

	global_devfs = devfs;

	struct vfs *vfs = vfs_mount(devfs, mount_path, devfs);
	if (!vfs) {
		error("devfs_vfs_init: mount failed\n");
		return -1;
	}

	memcpy(vfs->root_vnode->ops, &devfs_vnops, sizeof(struct vfs_vops));

	devfs_node_t *root = devfs_create_node(DEVFS_TYPE_DIR);
	if (!root) {
		error("devfs_vfs_init: root alloc failed\n");
		return -1;
	}

	root->name = strdup("/");
	root->device = NULL;

	devfs->root_node = root;

	struct device *dev = device_get_list();

	while (dev) {
		if (dev->dev_node_path) {
			devfs_node_t *n = devfs_create_node(DEVFS_TYPE_CHAR);

			if (!n) {
				error("devfs_vfs_init: node alloc failed\n");
				return -1;
			}

			n->name = strdup(dev->dev_node_path);
			n->device = dev;

			devfs_append_child(root, n);
		}

		dev = dev->next;
	}

	return 0;
}
