/*********************************************************************************/
/* Module Name:  newc.c */
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

#include <fs/cpio/newc.h>
#include <mm/heap.h>
#include <debug/log.h>
#include <lib/string.h>
#include <user/access.h>

#define align4(x) (((x) + 3) & ~3)

typedef struct {
	uint8_t *start;
	uint8_t *pos;
	uint8_t *end;
} cpio_reader_t;

static uint64_t parse_hex(char *buf, size_t len)
{
	char temp[17] = { 0 };
	memcpy(temp, buf, len);
	return strtoull(temp, NULL, 16);
}

static int cpio_reader_next(cpio_reader_t *reader, struct cpio_file *file)
{
	if ((size_t)(reader->end - reader->pos) < 110) {
		warn("cpio: truncated header at 0x%llx (remaining=%zu)\n",
			 (unsigned long long)(reader->pos - reader->start),
			 (size_t)(reader->end - reader->pos));
		return -1;
	}

	if (memcmp(reader->pos, "070701", 6) != 0 &&
		memcmp(reader->pos, "070702", 6) != 0) {
		warn("cpio: invalid magic at 0x%llx\n",
			 (unsigned long long)(reader->pos - reader->start));
		return -1;
	}

	uint8_t *pos = reader->pos + 6;

	file->ino = parse_hex((char *)pos, 8);
	pos += 8;
	file->mode = parse_hex((char *)pos, 8);
	pos += 8;
	file->uid = parse_hex((char *)pos, 8);
	pos += 8;
	file->gid = parse_hex((char *)pos, 8);
	pos += 8;
	file->nlink = parse_hex((char *)pos, 8);
	pos += 8;
	file->mtime = parse_hex((char *)pos, 8);
	pos += 8;
	file->filesize = parse_hex((char *)pos, 8);
	pos += 8;
	file->devmajor = parse_hex((char *)pos, 8);
	pos += 8;
	file->devminor = parse_hex((char *)pos, 8);
	pos += 8;
	file->rdevmajor = parse_hex((char *)pos, 8);
	pos += 8;
	file->rdevminor = parse_hex((char *)pos, 8);
	pos += 8;
	file->namesize = parse_hex((char *)pos, 8);
	pos += 8;
	file->check = parse_hex((char *)pos, 8);
	pos += 8;

	reader->pos = pos;

	if (file->namesize == 0 ||
		(size_t)(reader->end - reader->pos) < file->namesize) {
		warn("cpio: invalid namesize %llu at 0x%llx (remaining=%zu)\n",
			 (unsigned long long)file->namesize,
			 (unsigned long long)(reader->pos - reader->start),
			 (size_t)(reader->end - reader->pos));
		return -1;
	}

	file->filename = (char *)reader->pos;
	reader->pos += file->namesize;
	reader->pos = (uint8_t *)align4((uintptr_t)reader->pos);

	if (file->filename[file->namesize - 1] != '\0') {
		warn("cpio: filename missing NUL terminator\n");
		return -1;
	}

	if (strcmp(file->filename, "TRAILER!!!") == 0)
		return 1;

	if (file->filesize) {
		if ((size_t)(reader->end - reader->pos) < file->filesize) {
			warn("cpio: truncated data for %s (size=%llu, remaining=%zu)\n",
				 file->filename, (unsigned long long)file->filesize,
				 (size_t)(reader->end - reader->pos));
			return -1;
		}
		file->data = reader->pos;
		reader->pos += file->filesize;
		reader->pos = (uint8_t *)align4((uintptr_t)reader->pos);
		if (reader->pos > reader->end) {
			warn("cpio: data alignment moved past end for %s\n",
				 file->filename);
			return -1;
		}
	} else {
		file->data = NULL;
	}

	trace("cpio: entry %s mode=0x%llx size=%llu\n", file->filename,
		  (unsigned long long)file->mode, (unsigned long long)file->filesize);

	return 0;
}

int cpio_fs_parse(struct cpio_fs *fs, void *data, size_t size)
{
	if (!fs || !data || size < 110) {
		warn("cpio: invalid archive (%p, %p, size=%zu)\n", fs, data, size);
		return -1;
	}

	cpio_reader_t reader = {
		.start = (uint8_t *)data,
		.pos = (uint8_t *)data,
		.end = (uint8_t *)data + size,
	};

	fs->files = NULL;
	fs->file_count = 0;
	fs->archive_data = data;
	fs->archive_size = size;

	size_t capacity = 4;
	fs->files = kmalloc(capacity * sizeof(struct cpio_file));
	if (!fs->files)
		return -1;

	while (reader.pos < reader.end) {
		if (fs->file_count == capacity) {
			capacity *= 2;
			struct cpio_file *new_files =
				krealloc(fs->files, capacity * sizeof(struct cpio_file));
			if (!new_files)
				return -1;
			fs->files = new_files;
		}

		struct cpio_file *file = &fs->files[fs->file_count];
		memset(file, 0, sizeof(struct cpio_file));

		int res = cpio_reader_next(&reader, file);
		if (res == 1)
			break;
		if (res < 0) {
			warn("cpio: parse failed at index %zu (offset=0x%llx)\n",
				 fs->file_count,
				 (unsigned long long)(reader.pos - reader.start));
			return -1;
		}

		fs->file_count++;
	}

	trace("cpio: parsed %zu entries\n", fs->file_count);
	return 0;
}

size_t cpio_fs_read(struct cpio_fs *fs, const char *filename, void *buffer,
					size_t bufsize)
{
	for (size_t i = 0; i < fs->file_count; ++i) {
		if (strcmp(fs->files[i].filename, filename) == 0) {
			size_t len = fs->files[i].filesize;
			if (len > bufsize)
				len = bufsize;
			memcpy(buffer, fs->files[i].data, len);
			return len;
		}
	}
	return 0;
}

struct cpio_file *cpio_fs_get_file(struct cpio_fs *fs, const char *filename)
{
	for (size_t i = 0; i < fs->file_count; ++i) {
		if (strcmp(fs->files[i].filename, filename) == 0) {
			return &fs->files[i];
		}
	}
	return NULL;
}

void cpio_fs_free(struct cpio_fs *fs)
{
	kfree(fs->files);
	fs->files = NULL;
	fs->file_count = 0;
}

static const char *normalize_dest(const char *dest)
{
	if (strcmp(dest, "/") == 0)
		return "";
	return dest;
}

int cpio_extract(struct cpio_fs *cpio, char *dest_path)
{
	if (!cpio || !dest_path) {
		warn("Missing CPIO archive, or destination path (%p, %p).\n", cpio,
			 dest_path);
		return -1;
	}

	const char *base = normalize_dest(dest_path);

	size_t s = 20;
	char *path = kmalloc(s);
	memset(path, 0, s);
	if ((strlen(dest_path) + 1) > s) {
		path = krealloc(path, (strlen(dest_path) + 1));
		s = (strlen(dest_path) + 1);
	}

	trace("Extracting CPIO to %s\n", dest_path);

	if (strcmp(dest_path, "/") != 0) {
		vfs_mkdir(dest_path, 0755);
	}

	for (size_t i = 0; i < cpio->file_count; i++) {
		memset(path, 0, s);
		strcat(path, base);

		struct cpio_file *file = &cpio->files[i];

		debug("CPIO path %s\n", file->filename);

		char *fname = file->filename;
		if (fname[0] == '/')
			fname++;

		char *name_dup = strdup(fname);

		if (file->namesize + 1 > s) {
			s = file->namesize + 1;
			path = krealloc(path, s);
		}

		char *save;
		char *dir = strtok_r(name_dup, "/", &save);
		uint16_t type = file->mode & S_IFMT;

		while (dir) {
			if (*dir == '\0') {
				dir = strtok_r(NULL, "/", &save);
				continue;
			}

			bool is_last = !(save && *save);

			if (strlen(path) + strlen(dir) + 2 > s) {
				s = strlen(path) + strlen(dir) + 2;
				path = krealloc(path, s);
			}

			strcat(path, "/");
			strcat(path, dir);

			int flags = V_CREATE;
			if (!is_last)
				flags |= V_DIR;

			struct vnode *v;
			struct fileio *f;

			if (vfs_lookup(path, &v) != 0) {
				char *dup = strdup(path);
				if (!is_last || type == S_IFDIR) {
					if (vfs_mkdir(dup, 0755) != 0)
						warn("cpio: mkdir failed for %s\n", dup);
					if (vfs_lookup(dup, &v) != 0) {
						warn("cpio: lookup failed after mkdir for %s\n", dup);
						return -1;
					} else {
						v->gid = file->gid;
						v->uid = file->uid;
					}
				} else if (type == S_IFLNK) {
					if (!file->data || file->filesize == 0) {
						warn("cpio: empty symlink target for %s\n", dup);
						return -1;
					}
					char *target = kmalloc(file->filesize + 1);
					if (!target)
						return -1;
					memcpy(target, file->data, file->filesize);
					target[file->filesize] = '\0';
					if (vfs_symlink(target, dup) != 0) {
						kfree(target);
						warn("cpio: failed to create symlink %s -> %s\n", dup,
							 target);
						return -1;
					}
					kfree(target);
				} else if (type == S_IFREG) {
					if (vfs_create(dup, file->mode) != 0) {
						warn("cpio: create failed for %s\n", dup);
					} else {
						if (vfs_open(dup, 0, &f) == 0) {
							write(f, file->data, file->filesize);
							close(f);
						} else {
							warn("cpio: open failed for %s\n", dup);
						}
						if (vfs_lookup(dup, &v) != 0) {
							warn("cpio: lookup failed after create for %s\n",
								 dup);
							return -1;
						} else {
							v->gid = file->gid;
							v->uid = file->uid;
						}
					}
				} else {
					warn("cpio: unsupported type 0x%x for %s\n", type, dup);
				}
			}

			dir = strtok_r(NULL, "/", &save);
		}

		kfree(name_dup);
	}

	return 0;
}

int cpio_ramfs_init(struct cpio_fs *fs, struct ramfs *ramfs)
{
	if (!fs || !ramfs) {
		warn("Missing cpio archive or ramfs root struct!\n");
		return -1;
	}

	struct ramfs_node *cur_node;
	struct ramfs_node *next_node;

	for (size_t i = 0; i < fs->file_count; i++) {
		struct cpio_file *file = &fs->files[i];

		char *name_dup = strdup(file->filename);
		char *temp = name_dup;
		char *dir;

		for (int j = 0; *temp; j++) {
			dir = strtok_r(NULL, "/", &temp);
			enum ramfs_ftype rt;
			if (*temp) {
				rt = RAMFS_DIRECTORY;
			} else {
				rt = RAMFS_FILE;
			}

			switch (ramfs_find_or_create_node(ramfs, dir, rt, &next_node)) {
			case 0:
				break;

			case 1:
				next_node->name = strdup(dir);
				if (rt == RAMFS_FILE) {
					next_node->size = file->filesize;
					next_node->data = file->data;
				}

				if (j == 0 && i == 0) {
					cur_node = next_node;
					ramfs->root_node = cur_node;

					continue;
				}

				break;

			default:
				return -1;
			}

			if (j == 0 && cur_node != next_node) {
				cur_node->sibling = next_node;
				cur_node = cur_node->sibling;
			} else if (j > 0) {
				ramfs_append_child(cur_node, next_node);
				if (*temp)
					cur_node = next_node;
			}
		}

		kfree(name_dup);
	}

	return 0;
}
