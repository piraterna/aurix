/*********************************************************************************/
/* Module Name:  ramfs.c */
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

#include <fs/ramfs.h>
#include <lib/string.h>
#include <debug/log.h>
#include <util/kprintf.h>
#include <mm/heap.h>
#include <user/access.h>

struct ramfs *ramfs_create_fs()
{
	struct ramfs *ramfs = kmalloc(sizeof(struct ramfs));
	memset(ramfs, 0, sizeof(struct ramfs));
	return ramfs;
}

struct ramfs_node *ramfs_create_node(enum ramfs_ftype ftype)
{
	struct ramfs_node *node = kmalloc(sizeof(struct ramfs_node));
	memset(node, 0, sizeof(struct ramfs_node));
	node->type = ftype;

	switch (ftype) {
	case RAMFS_DIRECTORY:
		node->mode = S_IFDIR | 0755;
		break;
	case RAMFS_FILE:
		node->mode = S_IFREG | 0644;
		break;
	case RAMFS_SYMLINK:
		node->mode = S_IFLNK | 0777;
		break;
	}

	return node;
}
int ramfs_find_node(struct ramfs *ramfs, char *path, struct ramfs_node **out)
{
	if (!ramfs || !path || !out) {
		warn("ramfs_find_node: invalid arguments");
		return -1;
	}

	*out = NULL;

	if (!ramfs->root_node) {
		warn("ramfs_find_node: root node missing");
		return -1;
	}

	if (path[0] == '/')
		path++;

	if (path[0] == '\0') {
		*out = ramfs->root_node;
		return 0;
	}

	char *name_dup = strdup(path);
	if (!name_dup) {
		warn("ramfs_find_node: strdup failed");
		return -1;
	}

	char *saveptr = NULL;
	char *dir = strtok_r(name_dup, "/", &saveptr);

	struct ramfs_node *cur_node = ramfs->root_node;

	while (dir) {
		struct ramfs_node *child = cur_node->child;
		struct ramfs_node *match = NULL;

		while (child) {
			if (strcmp(child->name, dir) == 0) {
				match = child;
				break;
			}
			child = child->sibling;
		}

		if (!match) {
			warn("ramfs_find_node: component '%s' not found", dir);
			kfree(name_dup);
			return -1;
		}

		cur_node = match;
		dir = strtok_r(NULL, "/", &saveptr);
	}

	kfree(name_dup);
	*out = cur_node;

	return 0;
}

int ramfs_node_add(struct ramfs *ramfs, char *path, struct ramfs_node **out)
{
	if (!ramfs || !ramfs->root_node || !path) {
		return -1;
	}

	if (path[0] == '/') {
		path++;
	}

	char *name_dup = strdup(path);
	char *temp = name_dup;
	char *dir;

	struct ramfs_node *cur_node = ramfs->root_node;
	enum ramfs_ftype rt;

	// j = level of current node
	for (int j = 0; *temp; j++) {
		(void)(j);
		dir = strtok_r(NULL, "/", &temp);

		for (; cur_node != NULL; cur_node = cur_node->sibling) {
			if (*temp) {
				rt = RAMFS_DIRECTORY;
			} else {
				rt = RAMFS_FILE;
			}

			if (strcmp(cur_node->name, dir) != 0) {
				if (cur_node->sibling) {
					continue;
				}

				// finally, create the entry
				struct ramfs_node *n = ramfs_create_node(rt);
				n->name = strdup(dir);
				continue;
			}

			if (cur_node->type != rt) {
				continue;
			}

			// we should be fine and we can exit the loop
			break;
		}

		// go to the child
		if (*temp) {
			if (strchr(temp, '/')) {
				rt = RAMFS_DIRECTORY;
			} else {
				rt = RAMFS_FILE;
			}

			if (!cur_node->child) {
				struct ramfs_node *n = ramfs_create_node(rt);
				n->name = strdup(temp);
				ramfs_append_child(cur_node, n);
			}

			cur_node = cur_node->child;
		} else {
			*out = cur_node;
		}

		if (!cur_node) {
			// something went really wrong lol
			return -1;
		}
	}

	return 0;
}

// @returns 0 if a node was found
// @returns 1 if a node wasn't found and it has been created
int ramfs_find_or_create_node(struct ramfs *ramfs, char *path,
							  enum ramfs_ftype ramfs_ftype,
							  struct ramfs_node **out)
{
	struct ramfs_node *found = NULL;
	ramfs_find_node(ramfs, path, &found);

	if (!found) {
		*out = ramfs_create_node(ramfs_ftype);
		return 1;
	}

	*out = found;
	return 0;
}

// appends a node to a list of a parent's children
int ramfs_append_child(struct ramfs_node *parent, struct ramfs_node *child)
{
	if (!parent->child) {
		parent->child = child;
		return 0;
	}

	struct ramfs_node *last_child;
	for (last_child = parent->child; last_child->sibling != NULL;
		 last_child = last_child->sibling)
		;
	last_child->sibling = child;

	return 0;
}

int ramfs_print(struct ramfs_node *node, int lvl)
{
	if (!node) {
		return -1;
	}

	int a = lvl;

	while (a--) {
		kprintf("  ");
	}

	switch (node->type) {
	case RAMFS_DIRECTORY:
		kprintf("+ %-20s", node->name);
		break;

	case RAMFS_FILE:
		kprintf("  %-20s", node->name);
		kprintf(" | % 10zuB", node->size, node->data);
		break;

	case RAMFS_SYMLINK:
		kprintf("  %-20s -> %s", node->name, (char *)node->data);
		break;
	}

	kprintf("\n");

	if (node->child) {
		ramfs_print(node->child, lvl + 1);
	}

	if (node->sibling) {
		ramfs_print(node->sibling, lvl);
	}
	return 0;
}

size_t ramfs_get_node_size(struct ramfs_node *node)
{
	if (!node) {
		return -1;
	}

	size_t s = 0;

	switch (node->type) {
	case RAMFS_FILE:
		s = node->size;
		break;

	case RAMFS_DIRECTORY:
		for (struct ramfs_node *n = node->child; n != NULL; n = n->sibling) {
			s += ramfs_get_node_size(n);
		}
		break;

	case RAMFS_SYMLINK:
		s = strlen((char *)node->data) + 1;
		break;
	}

	return s;
}

int ramfs_open(struct vnode **vnode_r, int flags, bool clone,
			   struct fileio **fio_out)
{
	(void)(clone);

	// TODO: flags
	// TODO: clone

	// 1. find the file in the RAMFS

	struct vnode *vnode = *vnode_r;
	if (!vnode || !vnode_r) {
		return -1;
	}

	// vfs_data should have the root RAMFS struct
	struct ramfs *ramfs = vnode->root_vfs->vfs_data;

	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *ramfs_node;
	ramfs_find_node(ramfs, vnode->path, &ramfs_node);

	if (!ramfs_node) {
		ramfs_node = (struct ramfs_node *)vnode->node_data;

		if (!ramfs_node) {
			if (!(flags & V_CREATE)) {
				return -1;
			}

			// create the node
			if (ramfs_node_add(ramfs, vnode->path, &ramfs_node) != 0) {
				return -1;
			}

			if (flags & V_DIR) {
				ramfs_node->type = RAMFS_DIRECTORY;
				ramfs_node->size = 0;
				ramfs_node->data = NULL;
			}
		}
	}

	// this is the RAMFS node to be attached to the vnode
	struct ramfs_node *v_ramfs_node = kmalloc(sizeof(struct ramfs_node));
	memcpy(v_ramfs_node, ramfs_node, sizeof(struct ramfs_node));

	// 2. create a buffer for the file (do not use the original file buffer)

	if (v_ramfs_node->type == RAMFS_DIRECTORY) {
		v_ramfs_node->size = 0;
		v_ramfs_node->data = NULL;
		v_ramfs_node->child = ramfs_node->child;
		v_ramfs_node->sibling = ramfs_node->sibling;
		kfree(v_ramfs_node);
		vnode->node_data = ramfs_node;
	} else {
		v_ramfs_node->size = ramfs_get_node_size(ramfs_node);
		if (v_ramfs_node->size == 0) {
			v_ramfs_node->data = NULL;
		} else {
			v_ramfs_node->data = kmalloc(v_ramfs_node->size);
			if (!v_ramfs_node->data)
				return -1;
			if (v_ramfs_node->type == RAMFS_SYMLINK) {
				memcpy(v_ramfs_node->data, ramfs_node->data,
					   v_ramfs_node->size);
			} else {
				memcpy(v_ramfs_node->data, ramfs_node->data,
					   v_ramfs_node->size);
			}
		}
		v_ramfs_node->child = NULL;
		v_ramfs_node->sibling = NULL;
		vnode->node_data = v_ramfs_node;
	}

	struct fileio *fio = *fio_out;
	if (!fio || !fio_out) {
		return -1;
	}

	fio->buf_start = v_ramfs_node->data;
	fio->size = v_ramfs_node->size;
	fio->private = vnode;

	if (vnode->path && strcmp(vnode->path, "/sys/klog") == 0) {
		fio->size = klog_get_size();
	}

	return 0;
}

int ramfs_read(struct vnode *vn, size_t *bytes, size_t *offset, void *out)
{
	if (!vn) {
		return -1;
	}

	if (vn->path && strcmp(vn->path, "/sys/klog") == 0) {
		size_t read_bytes = klog_read_at(out, *bytes, *offset);
		*bytes = read_bytes;
		*offset += read_bytes;
		return 0;
	}

	memset(out, 0, (*bytes));

	struct ramfs_node *ramfs_node = (struct ramfs_node *)vn->node_data;
	if (!ramfs_node) {
		return -1;
	}

	if ((*bytes) > ramfs_node->size) {
		(*bytes) = ramfs_node->size;
	} else if ((*offset) >= ramfs_node->size) {
		return -1;
	}

	if ((*bytes) + (*offset) > ramfs_node->size) {
		// read whatever remains that can be copied
		(*bytes) = (ramfs_node->size - (*offset));
	}

	void *src = ramfs_node->data + (*offset);
	memcpy(out, src, (*bytes));
	*offset += *bytes;

	return 0;
}

int ramfs_write(struct vnode *vn, void *buf, size_t *bytes, size_t *offset)
{
	if (!vn) {
		return -1;
	}

	struct ramfs_node *ramfs_node = vn->node_data;
	if (!ramfs_node) {
		return -1;
	}

	if ((*bytes) + (*offset) > ramfs_node->size) {
		// we need to do some relocation
		size_t more = ((*bytes) + (*offset)) - ramfs_node->size;

		void *new_data = krealloc(ramfs_node->data, ramfs_node->size + more);
		ramfs_node->data = new_data;
		ramfs_node->size += more;
	}

	void *dst = ramfs_node->data + (*offset);
	memcpy(dst, buf, (*bytes));
	*offset += *bytes;

	if (vn->path && strcmp(vn->path, "/sys/klog") == 0) {
		struct ramfs *ramfs = vn->root_vfs->vfs_data;
		if (ramfs) {
			struct ramfs_node *ramfs_node_original = NULL;
			char *path = vn->path + strlen(vn->root_vfs->root_vnode->path);
			if (path[0] == '/')
				path++;
			ramfs_find_node(ramfs, path, &ramfs_node_original);
			if (ramfs_node_original && ramfs_node_original != ramfs_node) {
				if (ramfs_node_original->data != ramfs_node->data) {
					kfree(ramfs_node_original->data);
					ramfs_node_original->data = ramfs_node->data;
				}
				ramfs_node_original->size = ramfs_node->size;
			}
		}
	}

	return 0;
}

int ramfs_close(struct vnode *vnode, int flags, bool clone)
{
	(void)(clone);

	// TODO: flags
	// TODO: clone

	if (!vnode) {
		return -1;
	}

	struct ramfs_node *ramfs_node = vnode->node_data;
	if (!ramfs_node) {
		return -1;
	}

	// we'll sync the new buffer

	struct ramfs *ramfs = vnode->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *ramfs_node_original;
	char *path = vnode->path + strlen(vnode->root_vfs->root_vnode->path);
	ramfs_find_node(ramfs, path, &ramfs_node_original);

	if (!ramfs_node_original) {
		return -1; // this file is probably not ours
	}

	if (ramfs_node_original == ramfs_node) {
		return 0;
	}

	if (!(flags & (O_WRONLY | O_RDWR | O_APPEND | O_CREATE | O_TRUNC))) {
		if (ramfs_node->data && ramfs_node->data != ramfs_node_original->data)
			kfree(ramfs_node->data);
		kfree(vnode->node_data);
		return 0;
	}

	if (ramfs_node_original->data != ramfs_node->data) {
		kfree(ramfs_node_original->data);
		// the old node points to the original (now probably updated) data
		ramfs_node_original->data = ramfs_node->data;
		ramfs_node_original->size = ramfs_node->size;
	}

	// get rid of the RAMFS node on vnode
	kfree(vnode->node_data);

	return 0;
}

int ramfs_ioctl(struct vnode *vnode, int request, void *arg)
{
	if (!vnode) {
		return -1;
	}

	(void)(request);
	(void)(arg);

	return -1;
}

int ramfs_lookup(struct vnode *parent, const char *name, struct vnode **out)
{
	if (!parent || !name || !out) {
		return -1;
	}

	struct ramfs *ramfs = parent->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *parent_node = parent->node_data;

	if (!parent_node) {
		char *rel_path =
			parent->path + strlen(parent->root_vfs->root_vnode->path);
		if (rel_path[0] == '/')
			rel_path++;

		if (rel_path[0] == '\0' ||
			strcmp(parent->path, parent->root_vfs->root_vnode->path) == 0) {
			parent_node = ramfs->root_node;
		} else {
			ramfs_find_node(ramfs, rel_path, &parent_node);
		}
	}

	if (!parent_node) {
		return -1;
	}

	if (parent_node->type != RAMFS_DIRECTORY) {
		return -1;
	}

	for (struct ramfs_node *child = parent_node->child; child != NULL;
		 child = child->sibling) {
		if (strcmp(child->name, name) == 0) {
			size_t parent_len = strlen(parent->path);
			size_t child_len = strlen(name);
			char *child_path = kmalloc(parent_len + child_len + 2);
			strcpy(child_path, parent->path);
			if (child_path[parent_len - 1] != '/') {
				strcat(child_path, "/");
			}
			strcat(child_path, name);

			enum vnode_type vtype = VNODE_REGULAR;
			if (child->type == RAMFS_DIRECTORY)
				vtype = VNODE_DIR;
			else if (child->type == RAMFS_SYMLINK)
				vtype = VNODE_LINK;

			struct vnode *child_vnode =
				vnode_create(parent->root_vfs, child_path, vtype, child);
			child_vnode->mode = child->mode;
			child_vnode->uid = 0;
			child_vnode->gid = 0;
			memcpy(child_vnode->ops, parent->ops, sizeof(struct vnode_ops));

			*out = child_vnode;
			return 0;
		}
	}

	return -1;
}

int ramfs_readdir(struct vnode *vnode, struct dirent *entries, size_t *count)
{
	if (!vnode || !entries || !count) {
		return -1;
	}

	if (vnode->vtype != VNODE_DIR) {
		return -1;
	}

	struct ramfs *ramfs = vnode->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *dir_node = vnode->node_data;
	if (!dir_node) {
		char *rel_path =
			vnode->path + strlen(vnode->root_vfs->root_vnode->path);
		if (rel_path[0] == '/')
			rel_path++;
		ramfs_find_node(ramfs, rel_path, &dir_node);
	}

	if (!dir_node || dir_node->type != RAMFS_DIRECTORY) {
		return -1;
	}

	size_t idx = 0;
	size_t max = *count;

	for (struct ramfs_node *child = dir_node->child; child != NULL && idx < max;
		 child = child->sibling) {
		entries[idx].d_ino = (uint64_t)child;
		entries[idx].d_off = idx + 1;
		entries[idx].d_reclen = sizeof(struct dirent);

		if (child->type == RAMFS_DIRECTORY) {
			entries[idx].d_type = DT_DIR;
		} else if (child->type == RAMFS_SYMLINK) {
			entries[idx].d_type = DT_LNK;
		} else {
			entries[idx].d_type = DT_REG;
		}

		strncpy(entries[idx].d_name, child->name,
				sizeof(entries[idx].d_name) - 1);
		entries[idx].d_name[sizeof(entries[idx].d_name) - 1] = '\0';

		idx++;
	}

	*count = idx;
	return 0;
}

int ramfs_readlink(struct vnode *vnode, char *buf, size_t size)
{
	if (!vnode || !buf) {
		return -1;
	}

	if (vnode->vtype != VNODE_LINK) {
		return -1;
	}

	struct ramfs_node *link_node = vnode->node_data;
	if (!link_node || link_node->type != RAMFS_SYMLINK) {
		return -1;
	}

	if (!link_node->data) {
		return -1;
	}

	size_t target_len = strlen((char *)link_node->data);
	size_t copy_len = target_len < size - 1 ? target_len : size - 1;

	memcpy(buf, link_node->data, copy_len);
	buf[copy_len] = '\0';

	return 0;
}

int ramfs_mkdir(struct vnode *parent, const char *name, int mode)
{
	(void)(mode);

	if (!parent || !name) {
		return -1;
	}

	struct ramfs *ramfs = parent->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *parent_node = parent->node_data;
	if (!parent_node) {
		char *rel_path =
			parent->path + strlen(parent->root_vfs->root_vnode->path);
		if (rel_path[0] == '/')
			rel_path++;
		ramfs_find_node(ramfs, rel_path, &parent_node);
	}

	if (!parent_node || parent_node->type != RAMFS_DIRECTORY) {
		return -1;
	}

	for (struct ramfs_node *child = parent_node->child; child != NULL;
		 child = child->sibling) {
		if (strcmp(child->name, name) == 0) {
			return -1;
		}
	}

	struct ramfs_node *new_dir = ramfs_create_node(RAMFS_DIRECTORY);
	new_dir->name = strdup(name);
	new_dir->mode = S_IFDIR | (mode & 0777);

	ramfs_append_child(parent_node, new_dir);
	return 0;
}

int ramfs_rmdir(struct vnode *parent, const char *name)
{
	if (!parent || !name) {
		return -1;
	}

	struct ramfs *ramfs = parent->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *parent_node = parent->node_data;
	if (!parent_node) {
		char *rel_path =
			parent->path + strlen(parent->root_vfs->root_vnode->path);
		if (rel_path[0] == '/')
			rel_path++;
		ramfs_find_node(ramfs, rel_path, &parent_node);
	}

	if (!parent_node || parent_node->type != RAMFS_DIRECTORY) {
		return -1;
	}

	struct ramfs_node **prev = &parent_node->child;
	for (struct ramfs_node *child = parent_node->child; child != NULL;
		 prev = &child->sibling, child = child->sibling) {
		if (strcmp(child->name, name) == 0) {
			if (child->type != RAMFS_DIRECTORY) {
				return -1;
			}

			if (child->child != NULL) {
				return -1;
			}

			*prev = child->sibling;
			kfree(child->name);
			kfree(child);
			return 0;
		}
	}

	return -1;
}

int ramfs_create(struct vnode *parent, const char *name, mode_t mode,
				 struct vnode **out)
{
	if (!parent || !name || !out) {
		return -1;
	}

	struct ramfs *ramfs = parent->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *parent_node = parent->node_data;
	if (!parent_node) {
		char *rel_path =
			parent->path + strlen(parent->root_vfs->root_vnode->path);
		if (rel_path[0] == '/')
			rel_path++;
		ramfs_find_node(ramfs, rel_path, &parent_node);
	}

	if (!parent_node || parent_node->type != RAMFS_DIRECTORY) {
		return -1;
	}

	for (struct ramfs_node *child = parent_node->child; child != NULL;
		 child = child->sibling) {
		if (strcmp(child->name, name) == 0) {
			return -1;
		}
	}

	enum ramfs_ftype rt = RAMFS_FILE;

	struct ramfs_node *new_file = ramfs_create_node(rt);
	new_file->name = strdup(name);
	new_file->size = 0;
	new_file->data = NULL;
	new_file->mode = S_IFREG | mode;

	ramfs_append_child(parent_node, new_file);

	size_t parent_len = strlen(parent->path);
	size_t name_len = strlen(name);
	char *file_path = kmalloc(parent_len + name_len + 2);
	strcpy(file_path, parent->path);
	if (file_path[parent_len - 1] != '/') {
		strcat(file_path, "/");
	}
	strcat(file_path, name);

	struct vnode *file_vnode =
		vnode_create(parent->root_vfs, file_path, VNODE_REGULAR, new_file);
	memcpy(file_vnode->ops, parent->ops, sizeof(struct vnode_ops));
	file_vnode->mode = new_file->mode;

	file_vnode->uid = 0;
	file_vnode->gid = 0;

	*out = file_vnode;
	return 0;
}

int ramfs_remove(struct vnode *parent, const char *name)
{
	if (!parent || !name) {
		return -1;
	}

	struct ramfs *ramfs = parent->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *parent_node = parent->node_data;
	if (!parent_node) {
		char *rel_path =
			parent->path + strlen(parent->root_vfs->root_vnode->path);
		if (rel_path[0] == '/')
			rel_path++;
		ramfs_find_node(ramfs, rel_path, &parent_node);
	}

	if (!parent_node || parent_node->type != RAMFS_DIRECTORY) {
		return -1;
	}

	struct ramfs_node **prev = &parent_node->child;
	for (struct ramfs_node *child = parent_node->child; child != NULL;
		 prev = &child->sibling, child = child->sibling) {
		if (strcmp(child->name, name) == 0) {
			if (child->type == RAMFS_DIRECTORY) {
				return -1;
			}

			*prev = child->sibling;
			kfree(child->name);
			if (child->data)
				kfree(child->data);
			kfree(child);
			return 0;
		}
	}

	return -1;
}

int ramfs_symlink(struct vnode *parent, const char *name, const char *target)
{
	if (!parent || !name || !target) {
		return -1;
	}

	struct ramfs *ramfs = parent->root_vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	struct ramfs_node *parent_node = parent->node_data;
	if (!parent_node) {
		char *rel_path =
			parent->path + strlen(parent->root_vfs->root_vnode->path);
		if (rel_path[0] == '/')
			rel_path++;
		ramfs_find_node(ramfs, rel_path, &parent_node);
	}

	if (!parent_node || parent_node->type != RAMFS_DIRECTORY) {
		return -1;
	}

	for (struct ramfs_node *child = parent_node->child; child != NULL;
		 child = child->sibling) {
		if (strcmp(child->name, name) == 0) {
			return -1;
		}
	}

	struct ramfs_node *new_link = ramfs_create_node(RAMFS_SYMLINK);
	new_link->name = strdup(name);
	new_link->size = strlen(target) + 1;
	new_link->data = strdup(target);

	ramfs_append_child(parent_node, new_link);
	return 0;
}

int ramfs_getattr(struct vnode *vnode, struct stat *st)
{
	if (!vnode || !st) {
		return -1;
	}

	struct ramfs_node *ramfs_node = vnode->node_data;
	if (!ramfs_node) {
		return -1;
	}

	size_t node_size = ramfs_node->size;
	if (vnode->path && strcmp(vnode->path, "/sys/klog") == 0) {
		node_size = klog_get_size();
	}

	st->st_dev = 1;
	st->st_ino = (uint64_t)ramfs_node;
	st->st_nlink = 1;
	st->st_mode = ramfs_node->mode;
	st->st_uid = vnode->uid;
	st->st_gid = vnode->gid;
	st->st_rdev = 0;
	st->st_size = node_size;
	st->st_blksize = 4096;
	st->st_blocks = (node_size + 4095) / 4096;
	st->st_atim = st->st_mtim = st->st_ctim = 0;

	return 0;
}

int ramfs_setattr(struct vnode *vnode, struct stat *st)
{
	if (!vnode || !st) {
		return -1;
	}

	struct ramfs_node *ramfs_node = vnode->node_data;
	if (!ramfs_node) {
		return -1;
	}

	ramfs_node->mode = st->st_mode;
	vnode->uid = st->st_uid;
	vnode->gid = st->st_gid;

	return 0;
}

struct vnode_ops ramfs_vnops = {
	.open = ramfs_open,
	.close = ramfs_close,
	.read = ramfs_read,
	.write = ramfs_write,
	.ioctl = ramfs_ioctl,
	.lookup = ramfs_lookup,
	.readdir = ramfs_readdir,
	.readlink = ramfs_readlink,
	.mkdir = ramfs_mkdir,
	.rmdir = ramfs_rmdir,
	.create = ramfs_create,
	.remove = ramfs_remove,
	.symlink = ramfs_symlink,
	.getattr = ramfs_getattr,
	.setattr = ramfs_setattr,
};

static int ramfs_vfs_mount(struct vfs *vfs, char *path, void *data)
{
	(void)(path);
	(void)(data);

	if (!vfs) {
		return -1;
	}

	return 0;
}

static int ramfs_vfs_unmount(struct vfs *vfs)
{
	if (!vfs) {
		return -1;
	}

	struct ramfs *ramfs = vfs->vfs_data;
	if (ramfs && ramfs->root_node) {
		// TODO: free ramfs tree
	}

	return 0;
}

static int ramfs_vfs_root(struct vfs *vfs, struct vnode **out)
{
	if (!vfs || !out) {
		return -1;
	}

	*out = vfs->root_vnode;
	vnode_ref(*out);

	return 0;
}

static int ramfs_vfs_statfs(struct vfs *vfs, struct statfs *stat)
{
	if (!vfs || !stat) {
		return -1;
	}

	struct ramfs *ramfs = vfs->vfs_data;
	if (!ramfs) {
		return -1;
	}

	stat->block_size = 1;
	stat->total_blocks = ramfs->ramfs_size;
	stat->free_blocks = 0;
	stat->total_nodes = 0;
	stat->free_nodes = 0;

	return 0;
}

static int ramfs_vfs_sync(struct vfs *vfs)
{
	(void)(vfs);
	return 0;
}

struct vfs_ops ramfs_vfsops = {
	.mount = ramfs_vfs_mount,
	.unmount = ramfs_vfs_unmount,
	.root = ramfs_vfs_root,
	.statfs = ramfs_vfs_statfs,
	.sync = ramfs_vfs_sync,
};

static int ramfs_fstype_mount(void *device, char *mount_point, void *mount_data,
							  struct vfs **out)
{
	(void)(mount_data);

	struct ramfs *ramfs = (struct ramfs *)device;
	if (!ramfs) {
		ramfs = ramfs_create_fs();
		ramfs->root_node = ramfs_create_node(RAMFS_DIRECTORY);
		ramfs->root_node->name = strdup("/");
	}

	if (!ramfs->root_node) {
		ramfs->root_node = ramfs_create_node(RAMFS_DIRECTORY);
		ramfs->root_node->name = strdup("/");
	}

	struct vfs_fstype fstype;
	memset(&fstype, 0, sizeof(struct vfs_fstype));
	strncpy(fstype.name, "ramfs", sizeof(fstype.name) - 1);

	struct vfs *vfs = vfs_create_fs(&fstype, ramfs);
	if (!vfs) {
		return -1;
	}

	memcpy(vfs->ops, &ramfs_vfsops, sizeof(struct vfs_ops));

	vfs->root_vnode =
		vnode_create(vfs, mount_point, VNODE_DIR, ramfs->root_node);
	if (!vfs->root_vnode) {
		kfree(vfs->ops);
		kfree(vfs);
		return -1;
	}

	memcpy(vfs->root_vnode->ops, &ramfs_vnops, sizeof(struct vnode_ops));
	vfs->root_vnode->mode = ramfs->root_node->mode;
	vfs->root_vnode->uid = 0;
	vfs->root_vnode->gid = 0;
	*out = vfs;
	return 0;
}

static struct vfs_fstype ramfs_fstype = { .id = 0,
										  .name = "ramfs",
										  .mount = ramfs_fstype_mount,
										  .next = NULL };

void ramfs_init(void)
{
	vfs_register_fstype(&ramfs_fstype);
}

int ramfs_vfs_init(struct ramfs *ramfs, char *path)
{
	if (!path) {
		return -1;
	}

	struct vfs *vfs = vfs_mount(ramfs, "ramfs", path, NULL);
	if (!vfs) {
		return -1;
	}

	return 0;
}
