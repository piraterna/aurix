/*********************************************************************************/
/* Module Name:  access.h */
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

#ifndef _USER_ACCESS_H
#define _USER_ACCESS_H

#define R_OK 0x4
#define W_OK 0x2
#define X_OK 0x1

#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100

#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010

#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

#define S_IFDIR 0040000 /* Directory.  */
#define S_IFCHR 0020000 /* Character device.  */
#define S_IFBLK 0060000 /* Block device.  */
#define S_IFREG 0100000 /* Regular file.  */
#define S_IFIFO 0010000 /* FIFO.  */
#define S_IFLNK 0120000 /* Symbolic link.  */
#define S_IFSOCK 0140000 /* Socket.  */

#define S_IFMT 0170000 /* Type of file.  */

#define S_ISUID 0x800
#define S_ISGID 0x400

// TODO: check perms

#endif // _USER_ACCESS_H