/*********************************************************************************/
/* Module Name:  uefi_sfs.c */
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

#ifndef _FS_UEFI_SFS_H
#define _FS_UEFI_SFS_H

#include <stdint.h>
#include <vfs/drive.h>

struct vfs_drive *sfs_init(char *mountpoint);

size_t sfs_read(char *filename, char **buffer, size_t *size,
				struct vfs_drive *dev, void *fsdata);
uint8_t sfs_write(char *filename, char *buffer, size_t size,
				  struct vfs_drive *dev, void *fsdata);

#endif /* _FS_UEFI_SFS_H */
