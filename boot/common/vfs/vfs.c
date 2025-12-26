/*********************************************************************************/
/* Module Name:  vfs.c */
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

/* VFS originally written by Levente Kurusa, stripped down for AxBoot */
/* https://github.com/levex/osdev */

#include <lib/string.h>
#include <mm/mman.h>
#include <print.h>
#include <stddef.h>
#include <stdint.h>
#include <vfs/drive.h>
#include <vfs/vfs.h>

#define MAX_MOUNTS 32

struct vfs_drive *boot_drive = NULL;

int vfs_init(char *root_mountpoint)
{
	boot_drive = mount_boot_volume(root_mountpoint);
	if (boot_drive == NULL) {
		error("vfs_init(): Failed to allocate memory for VFS!\n");
		// fuck off and boot out early.
		return 0;
	}

	debug("vfs_init(): Mounted boot drive to \"\\\"\n");
	return 1;
}

size_t vfs_read(char *filename, char **buf, size_t *size)
{
	if (boot_drive->fs->read == NULL) {
		error("vfs_read(): Filesystem didn't set up a read function!\n");
		return 0;
	}

	return boot_drive->fs->read(filename, buf, size, boot_drive,
								boot_drive->fs->fsdata);
}

int vfs_write(char *filename, char *buf, size_t size)
{
	if (boot_drive->fs->write == NULL) {
		error("vfs_read(): Filesystem didn't setup a write function!\n");
		return 0;
	}

	return boot_drive->fs->write(filename, buf, size, boot_drive,
								 boot_drive->fs->fsdata);
}