/*********************************************************************************/
/* Module Name:  fileio.c */
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

#include <vfs/fileio.h>
#include <vfs/vfs.h>
#include <ipc/pipe.h>
#include <mm/heap.h>
#include <aurix.h>

#include <string.h>

int f2vflags(int fio_flags)
{
	int vflags = 0;

	if (fio_flags & O_CREATE) {
		vflags |= V_CREATE;
	}

	return vflags;
}

struct fileio *fio_create()
{
	struct fileio *fio = kmalloc(sizeof(struct fileio));
	memset(fio, 0, sizeof(struct fileio));

	return fio;
}

struct fileio *open(char *path, int flags)
{
	struct fileio *f = NULL;
	struct vfs *parent_vfs;
	if (!vfs_resolve_mount(path, &parent_vfs)) {
		return NULL;
	}

	if (!vfs_open(parent_vfs, path, f2vflags(flags), &f)) {
		return NULL;
	}

	f->offset = 0;
	f->flags = flags;

	return f;
}

size_t read(struct fileio *file, size_t size, void *out)
{
	if (!file) {
		error("fileio pointer is null\n");
		return -1;
	}

	if (file->flags & PIPE_READ_END) {
		pipe_read(file, out, &size);
		return size;
	} else if (file->flags & PIPE_WRITE_END) {
		return -1;
	}

	if (!(file->flags & SPECIAL_FILE_TYPE_DEVICE)) {
		if (file->offset >= file->size) {
			return 0;
		}

		if (size > file->size) {
			size = (file->size - file->offset);
		}
	}

	int ret = vfs_read((struct vnode *)file->private, size, file->offset, out);
	if (!ret) {
		return -1;
	}

	file->offset += size;

	return size;
}

int write(struct fileio *file, void *buf, size_t size)
{
	struct vnode *vn = file->private;

	if (file->flags & PIPE_WRITE_END) {
		pipe_write(file, buf, &size);
		return 0;
	} else if (file->flags & PIPE_READ_END) {
		return -1;
	}

	size_t offset = file->offset;
	if (file->flags & O_APPEND) {
		offset += file->size;
	}

	if (!vfs_write(vn, buf, size, offset)) {
		return -1;
	}

	file->offset += size;

	return 0;
}

int close(struct fileio *file)
{
	struct vnode *vn = file->private;

	if (file->flags & PIPE_READ_END || file->flags & PIPE_WRITE_END) {
		pipe_close(file);
		return 0;
	}

	if (!vfs_close(vn)) {
		return -1;
	}

	kfree(file);
	return 0;
}

size_t seek(struct fileio *file, size_t offset, uint8_t whence)
{
	if (!file) {
		error("file pointer is null\n");
		return -1;
	}

	switch (whence) {
	case SEEK_CUR:
		file->offset += offset;
		break;
	case SEEK_END:
		file->offset = (file->size + offset);
		break;
	case SEEK_SET:
		file->offset = offset;
	default:
		break;
	}

	return file->offset;
}

size_t fcntl(struct fileio *file, uint8_t cmd, void *arg)
{
	switch (cmd) {
	case F_GETFL:
		return file->flags;
	case F_SETFL:
		file->flags |= ((*(size_t *)arg) & (O_APPEND));
		break;
	default:
		break;
	}

	return 0;
}