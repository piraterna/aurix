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

struct devfs *global_devfs = NULL;

struct devfs *devfs_create()
{
	struct devfs *devfs = kmalloc(sizeof(struct devfs));
	if (!devfs) {
		error("devfs_create: kmalloc failed\n");
		return NULL;
	}

	memset(devfs, 0, sizeof(struct devfs));
	return devfs;
}

struct devfs_node *devfs_create_node(enum devfs_ftype ftype)
{
	struct devfs_node *node = kmalloc(sizeof(struct devfs_node));
	if (!node) {
		error("devfs_create_node: kmalloc failed\n");
		return NULL;
	}

	memset(node, 0, sizeof(struct devfs_node));
	node->type = ftype;

	return node;
}

int devfs_find_node(struct devfs *devfs, char *path, struct devfs_node **out)
{
	if (!devfs || !devfs->root_node || !path || !out) {
		error("devfs_find_node: invalid args\n");
		return -1;
	}

	*out = NULL;

	if (*path == '/')
		path++;

	if (*path == '\0') {
		*out = devfs->root_node;
		return 0;
	}

	char *dup = strdup(path);
	if (!dup) {
		error("devfs_find_node: strdup failed\n");
		return -1;
	}

	char *save;
	char *token = strtok_r(dup, "/", &save);

	struct devfs_node *cur = devfs->root_node;
	int depth = 0;

	while (token) {
		struct devfs_node *child = cur->child;

		if (!child) {
			warn("devfs_find_node: node '%s' has no children\n",
				 cur->name ? cur->name : "(root)");
		}

		struct devfs_node *tmp = child;
		int idx = 0;

		while (tmp) {
			tmp = tmp->sibling;
			idx++;
		}

		struct devfs_node *match = NULL;

		while (child) {
			if (child->name && strcmp(child->name, token) == 0) {
				match = child;
				break;
			}

			child = child->sibling;
		}

		if (!match) {
			warn("devfs_find_node: component '%s' NOT FOUND under '%s'\n",
				 token, cur->name ? cur->name : "(root)");

			kfree(dup);
			return -1;
		}

		cur = match;
		token = strtok_r(NULL, "/", &save);
		depth++;
	}

	kfree(dup);

	*out = cur;

	return 0;
}

struct devfs_node *devfs_find_child(struct devfs_node *parent, const char *name)
{
	if (!parent || !name)
		return NULL;

	struct devfs_node *child = parent->child;

	while (child) {
		if (child->name && strcmp(child->name, name) == 0)
			return child;

		child = child->sibling;
	}

	return NULL;
}

int devfs_append_child(struct devfs_node *parent, struct devfs_node *child)
{
	if (!parent->child) {
		parent->child = child;
		return 0;
	}

	struct devfs_node *last_child;
	for (last_child = parent->child; last_child->sibling != NULL;
		 last_child = last_child->sibling)
		;
	last_child->sibling = child;

	return 0;
}

int devfs_node_add(struct devfs *devfs, char *path, struct devfs_node **out)
{
	if (!devfs || !devfs->root_node || !path) {
		error("devfs_node_add: invalid args\n");
		return -1;
	}

	if (path[0] == '/')
		path++;

	char *name_dup = strdup(path);
	char *temp = name_dup;
	char *dir = NULL;

	struct devfs_node *cur_node = devfs->root_node;
	enum devfs_ftype rt;
	for (int j = 0; *temp; j++) {
		(void)j;
		dir = strtok_r(NULL, "/", &temp);

		for (; cur_node != NULL; cur_node = cur_node->sibling) {
			if (*temp) {
				rt = DEVFS_TYPE_DIR;
			} else {
				for (int i = 0; i < device_count; i++) {
					if (strcmp(device_list[i]->dev_node_path, dir) == 0) {
						rt = DEVFS_TYPE_CHAR;
						break;
					}
				}
				rt = DEVFS_TYPE_FILE;
			}

			if (strcmp(cur_node->name, dir) != 0) {
				if (cur_node->sibling) {
					continue;
				}

				struct devfs_node *n = devfs_create_node(rt);
				n->name = strdup(dir);
				struct device *dev = device_list[0];
				for (int i = 0; i < device_count; i++) {
					if (strcmp(device_list[i]->dev_node_path, dir) == 0) {
						dev = device_list[i];
						break;
					}
				}
				n->device = dev;

				continue;
			}

			if (cur_node->type != rt) {
				continue;
			}

			break;
		}

		if (*temp) {
			if (strchr(temp, '/')) {
				rt = DEVFS_TYPE_DIR;
			} else {
				for (int i = 0; i < device_count; i++) {
					if (strcmp(device_list[i]->dev_node_path, dir) == 0) {
						rt = DEVFS_TYPE_CHAR;
						break;
					}
				}
				rt = DEVFS_TYPE_FILE;
			}

			if (!cur_node->child) {
				struct devfs_node *n = devfs_create_node(rt);
				n->name = strdup(temp);
				struct device *dev = device_list[0];
				for (int i = 0; i < device_count; i++) {
					if (strcmp(device_list[i]->dev_node_path, dir) == 0) {
						dev = device_list[i];
						break;
					}
				}
				n->device = dev;

				devfs_append_child(cur_node, n);
			}

			cur_node = cur_node->child;
		} else {
			*out = cur_node;
		}

		if (!cur_node) {
			return -1;
		}
	}

	return 0;
}

int devfs_refresh(struct devfs *devfs)
{
	if (!devfs || !devfs->root_node) {
		error("devfs_refresh: invalid args\n");
		return -1;
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
			struct devfs_node *new_node = devfs_create_node(DEVFS_TYPE_CHAR);
			if (!new_node) {
				error(
					"devfs_refresh: failed to allocate memory for new_node\n");
				return -1;
			}

			new_node->name = strdup(dev->dev_node_path);
			new_node->device = dev;

			if (!new_node->name) {
				kfree(new_node);
				error(
					"devfs_refresh: failed to allocate memory for new_node->name\n");
				return -1;
			}

			devfs_append_child(root, new_node);
		}
	}

	return 0;
}

int devfs_open(struct vnode **vnode, int flags, bool create,
			   struct fileio **out)
{
	(void)flags;
	(void)create;
	struct vnode *vn = *vnode;
	if (!vn || !vnode) {
		error("devfs_open: NULL vnode\n");
		return -1;
	}

	struct devfs *devfs = vn->vfs_root->fs_data;
	if (!devfs) {
		error("devfs_open: NULL devfs\n");
		return -1;
	}

	struct devfs_node *devfs_node;
	devfs_find_node(devfs, vn->path, &devfs_node);

	if (!devfs_node) {
		if (!(flags & V_CREATE)) {
			return -1;
		}

		if (devfs_node_add(devfs, vn->path, &devfs_node) != 0) {
			error("devfs_open: failed to add node\n");
			return -1;
		}
	}

	vn->node_data = devfs_node;

	struct fileio *fio_file = *out;
	if (!fio_file || !out) {
		error("devfs_open:  NULL fio\n");
		return -1;
	}

	fio_file->buf_start = NULL;
	fio_file->flags |= SPECIAL_FILE_TYPE_DEVICE;
	fio_file->size = 0;

	if (devfs_node && devfs_node->device && devfs_node->device->ops &&
		devfs_node->device->ops->open) {
		return devfs_node->device->ops->open(devfs_node->device);
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

	struct devfs_node *node = vn->node_data;
	if (node && node->device && node->device->ops && node->device->ops->close)
		return node->device->ops->close(node->device);

	return 0;
}

int devfs_read(struct vnode *vn, size_t *bytes, size_t *offset, void *out)
{
	if (!vn || !bytes || !offset || !out) {
		error("devfs_read: invalid args\n");
		return -1;
	}

	struct devfs_node *node = vn->node_data;

	if (!node || !node->device || !node->device->ops ||
		!node->device->ops->read) {
		error("devfs_read: read unsupported\n");
		return -1;
	}

	return node->device->ops->read(node->device, out, *bytes);
}

void print_device(struct devfs_node *node, int indent)
{
	if (!node)
		return;

	for (int i = 0; i < indent; i++)
		kprintf("  ");

	kprintf("struct devfs_node {\n");
	for (int i = 0; i <= indent; i++)
		kprintf("  ");
	kprintf("name = %s;\n", node->name ? node->name : "NULL");
	for (int i = 0; i <= indent; i++)
		kprintf("  ");
	kprintf("type = %d;\n", node->type);

	for (int i = 0; i <= indent; i++)
		kprintf("  ");
	kprintf("device = ");
	if (node->device) {
		kprintf("{\n");
		for (int j = 0; j <= indent + 1; j++)
			kprintf("  ");
		kprintf("name = %s;\n",
				node->device->name ? node->device->name : "NULL");
		for (int j = 0; j <= indent + 1; j++)
			kprintf("  ");
		kprintf("class_name = %s;\n",
				node->device->class_name ? node->device->class_name : "NULL");
		for (int j = 0; j <= indent + 1; j++)
			kprintf("  ");
		kprintf("dev_node_path = %s;\n", node->device->dev_node_path ?
											 node->device->dev_node_path :
											 "NULL");
		for (int j = 0; j <= indent + 1; j++)
			kprintf("  ");
		kprintf("driver_data = %p;\n", node->device->driver_data);
		for (int j = 0; j <= indent + 1; j++)
			kprintf("  ");
		kprintf("bound_driver = %p;\n", node->device->bound_driver);
		for (int j = 0; j <= indent + 1; j++)
			kprintf("  ");
		kprintf("ops = %p;\n", node->device->ops);
		for (int j = 0; j <= indent + 1; j++)
			kprintf("  ");
		kprintf("next = %p;\n", node->device->next);
		for (int j = 0; j <= indent; j++)
			kprintf("  ");
		kprintf("};\n");
	} else {
		kprintf("NULL;\n");
	}

	for (int i = 0; i <= indent; i++)
		kprintf("  ");
	kprintf("child = ");
	if (node->child) {
		print_device(node->child, indent + 1);
	} else {
		kprintf("NULL;\n");
	}

	for (int i = 0; i <= indent; i++)
		kprintf("  ");
	kprintf("sibling = ");
	if (node->sibling) {
		print_device(node->sibling, indent);
	} else {
		kprintf("NULL;\n");
	}

	for (int i = 0; i < indent; i++)
		kprintf("  ");
	kprintf("};\n");
}

int devfs_write(struct vnode *vn, void *buf, size_t *bytes, size_t *offset)
{
	if (!vn || !buf || !bytes || !offset) {
		error("devfs_write: invalid args\n");
		return -1;
	}

	struct devfs_node *node = vn->node_data;
	success("devfs: write(%s) reached\n", vn->path);

	print_device(node, 0);

	if (!node || !node->device || !node->device->ops ||
		!node->device->ops->write) {
		error("devfs_write: write unsupported\n");
		return -1;
	}

	debug("%p\n", node->device->ops);
	debug("%p\n", node->device->ops->write);

	return node->device->ops->write(node->device, buf, *bytes);
}

int devfs_ioctl(struct vnode *vnode, int request, void *arg)
{
	if (!vnode) {
		error("devfs_ioctl: vnode null\n");
		return -1;
	}

	struct devfs_node *node = vnode->node_data;

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

int devfs_vfs_init(struct devfs *devfs, char *mount_path)
{
	if (!devfs || !mount_path) {
		error("devfs_vfs_init: invalid args\n");
		return -1;
	}

	struct vfs *vfs = vfs_mount(devfs, mount_path, devfs);
	if (!vfs) {
		error("devfs_vfs_init: mount failed\n");
		return -1;
	}

	memcpy(vfs->root_vnode->ops, &devfs_vnops, sizeof(struct vfs_vops));

	struct devfs_node *root_node = devfs_create_node(DEVFS_TYPE_DIR);
	if (!root_node) {
		error("devfs_vfs_init: failed to allocate root_node\n");
		return -1;
	}
	root_node->name = strdup("/");
	if (!root_node->name) {
		kfree(root_node);
		error("devfs_vfs_init: failed to allocate root_node->name\n");
		return -1;
	}
	vfs->root_vnode->node_data = root_node;
	devfs->root_node = root_node;
	devfs->devfs_size = 0;
	devfs->root_node->device = NULL;
	devfs->root_node->sibling = NULL;
	devfs->root_node->child = NULL;
	devfs->root_node->type = DEVFS_TYPE_DIR;

	global_devfs = devfs;

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
