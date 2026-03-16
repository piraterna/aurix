/*********************************************************************************/
/* Module Name:  newc.h */
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

#ifndef _FS_CPIO_NEWC_H
#define _FS_CPIO_NEWC_H

#include <stddef.h>
#include <stdint.h>
#include <fs/ramfs.h>

struct cpio_file {
	char cmagic[6];
	uint64_t ino;
	uint64_t mode;
	uint64_t uid;
	uint64_t gid;
	uint64_t nlink;
	uint64_t mtime;
	uint64_t filesize;
	uint64_t devmajor;
	uint64_t devminor;
	uint64_t rdevmajor;
	uint64_t rdevminor;
	uint64_t namesize;
	uint64_t check;
	char *filename;
	void *data;
};

struct cpio_fs {
	struct cpio_file *files;
	size_t file_count;
	void *archive_data;
	size_t archive_size;
};

int cpio_fs_parse(struct cpio_fs *fs, void *data, size_t size);
size_t cpio_fs_read(struct cpio_fs *fs, const char *filename, void *buffer,
					size_t bufsize);
void cpio_fs_free(struct cpio_fs *fs);
struct cpio_file *cpio_fs_get_file(struct cpio_fs *fs, const char *filename);
int cpio_ramfs_init(struct cpio_fs *fs, struct ramfs *ramfs);

#endif // _FS_CPIO_NEWC_H