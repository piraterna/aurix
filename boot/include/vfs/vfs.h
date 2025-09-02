/*********************************************************************************/
/* Module Name:  vfs.h */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#ifndef _VFS_VFS_H
#define _VFS_VFS_H

#include <stddef.h>
#include <stdint.h>
#include <vfs/drive.h>

struct vfs_filesystem {
	size_t (*read)(char *, char **, struct vfs_drive *, void *);
	uint8_t (*write)(char *, char *, size_t, struct vfs_drive *, void *);

	void *fsdata;
};

struct vfs_mount {
	char *mnt;
	struct vfs_drive *drive;
};

int vfs_init(char *root_mountpoint);

/* This function allocates `buf`. Passing a non-NULL value will result in an
 * error. */
/* NOTE: Remember to free the allocated memory afterwards! */
size_t vfs_read(char *filename, char **buf);
int vfs_write(char *filename, char *buf, size_t len);

/* Every platform will define this on its own */
struct vfs_drive *mount_boot_volume(char *mountpoint);

#endif /* _VFS_VFS_H */
