/*********************************************************************************/
/* Module Name:  fileio.h */
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

#ifndef _VFS_FILEIO_H
#define _VFS_FILEIO_H

#include <stddef.h>
#include <sys/types.h>

typedef enum fseek {
	SEEK_SET = 0,
	SEEK_CUR = 1,
	SEEK_END = 2,
} fseek_t;

typedef enum fcntl_cmd {
	F_GETFL = 0,
	F_SETFL = 1,
} fcntl_cmd_t;

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_ACCMODE 0x0003
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_NOCTTY 0x0100
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_NONBLOCK 0x0800
#define O_DSYNC 0x1000
#define O_SYNC 0x101000
#define O_DIRECTORY 0x10000
#define O_NOFOLLOW 0x20000
#define O_CLOEXEC 0x80000
#define O_CREATE O_CREAT

#define PIPE_READ_END (1 << 20)
#define PIPE_WRITE_END (1 << 21)

#define SPECIAL_FILE_TYPE_PIPE (1 << 4)
#define SPECIAL_FILE_TYPE_DEVICE (1 << 5)

struct fileio {
	void *buf_start;
	size_t size;
	size_t flags;
	size_t offset;
	void *private;
};

struct vnode;
struct dirent;

typedef struct dir_handle {
	struct vnode *vnode;
	struct dirent *entries;
	size_t count;
	size_t index;
	size_t syscall_ret_num;
} dir_handle_t;

struct fileio *fio_create();

struct fileio *open(const char *path, int flags, mode_t mode);
size_t read(struct fileio *file, size_t size, void *out);
int write(struct fileio *file, void *buf, size_t size);
int close(struct fileio *file);

size_t seek(struct fileio *file, size_t offset, fseek_t whence);

size_t fcntl(struct fileio *file, fcntl_cmd_t cmd, void *arg);

int fs_list(const char *path, int max_depth);

#endif /* _VFS_FILEIO_H */