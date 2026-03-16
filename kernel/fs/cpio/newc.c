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

#define align4(x) (((x) + 3) & ~3)

typedef struct {
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
		warn("cpio: Not enough data for header\n");
		return -1;
	}

	if (memcmp(reader->pos, "070701", 6) != 0 &&
		memcmp(reader->pos, "070702", 6) != 0) {
		warn("cpio: Invalid magic number\n");
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

	if ((size_t)(reader->end - reader->pos) < file->namesize) {
		warn("cpio: Not enough data for filename\n");
		return -1;
	}

	char *filename = kmalloc(file->namesize);
	if (!filename) {
		warn("cpio: Memory allocation failed for filename\n");
		return -1;
	}

	memcpy(filename, reader->pos, file->namesize);
	reader->pos += file->namesize;
	reader->pos = (uint8_t *)align4((uintptr_t)reader->pos);

	if (filename[file->namesize - 1] != '\0') {
		warn("cpio: Filename not null-terminated\n");
		kfree(filename);
		return -1;
	}

	if (strcmp(filename, "TRAILER!!!") == 0) {
		kfree(filename);
		return 1;
	}

	file->filename = filename;
	debug("cpio: Trying to allocate %llu bytes for file data '%s'\n",
		  file->filesize, file->filename);

	if ((size_t)(reader->end - reader->pos) < file->filesize) {
		warn("cpio: Not enough data for file content\n");
		return -1;
	}

	file->data = kmalloc(file->filesize);
	if (!file->data) {
		warn("cpio: Memory allocation failed for file data: %p\n");
		return -1;
	}

	memcpy(file->data, reader->pos, file->filesize);
	reader->pos += file->filesize;
	reader->pos = (uint8_t *)align4((uintptr_t)reader->pos);

	debug("cpio: Parsed file '%s' (size: %llu bytes)\n", file->filename,
		  file->filesize);

	return 0;
}

int cpio_fs_parse(struct cpio_fs *fs, void *data, size_t size)
{
	cpio_reader_t reader = {
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
		if (res < 0)
			return -1;

		fs->file_count++;
	}

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
	for (size_t i = 0; i < fs->file_count; ++i) {
		kfree(fs->files[i].filename);
		kfree(fs->files[i].data);
	}
	kfree(fs->files);
	fs->files = NULL;
	fs->file_count = 0;
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