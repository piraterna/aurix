/*********************************************************************************/
/* Module Name:  vfs.h */
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

#ifndef _VFS_FSID_H
#define _VFS_FSID_H

#include <stdint.h>

/*
    Straight out of the statfs man page :trollface:

    (we're not going to use like 98% of these but who knows)
*/

#define ADFS_SUPER_MAGIC 0xadf5
#define AFFS_SUPER_MAGIC 0xadff
#define AFS_SUPER_MAGIC 0x5346414f
#define ANON_INODE_FS_MAGIC \
	0x09041934 /* Anonymous inode FS (for pseudofiles that have no name e.g.,  \
                  epoll, signalfd, bpf) */
#define AUTOFS_SUPER_MAGIC 0x0187
#define BDEVFS_MAGIC 0x62646576
#define BEFS_SUPER_MAGIC 0x42465331
#define BFS_MAGIC 0x1badface
#define BINFMTFS_MAGIC 0x42494e4d
#define BPF_FS_MAGIC 0xcafe4a11
#define BTRFS_SUPER_MAGIC 0x9123683e
#define BTRFS_TEST_MAGIC 0x73727279
#define CGROUP_SUPER_MAGIC 0x27e0eb /* Cgroup pseudo FS */
#define CGROUP2_SUPER_MAGIC 0x63677270 /* Cgroup v2 pseudo FS */
#define CIFS_MAGIC_NUMBER 0xff534d42
#define CODA_SUPER_MAGIC 0x73757245
#define COH_SUPER_MAGIC 0x012ff7b7
#define CRAMFS_MAGIC 0x28cd3d45
#define DEBUGFS_MAGIC 0x64626720
#define DEVFS_SUPER_MAGIC 0x1373 /* Linux 2.6.17 and earlier */
#define DEVPTS_SUPER_MAGIC 0x1cd1
#define ECRYPTFS_SUPER_MAGIC 0xf15f
#define EFIVARFS_MAGIC 0xde5e81e4
#define EFS_SUPER_MAGIC 0x00414a53
#define EXT_SUPER_MAGIC 0x137d /* Linux 2.0 and earlier */
#define EXT2_OLD_SUPER_MAGIC 0xef51
#define EXT2_SUPER_MAGIC 0xef53
#define EXT3_SUPER_MAGIC 0xef53
#define EXT4_SUPER_MAGIC 0xef53
#define F2FS_SUPER_MAGIC 0xf2f52010
#define FUSE_SUPER_MAGIC 0x65735546
#define FUTEXFS_SUPER_MAGIC 0xbad1dea /* Unused */
#define HFS_SUPER_MAGIC 0x4244
#define HOSTFS_SUPER_MAGIC 0x00c0ffee
#define HPFS_SUPER_MAGIC 0xf995e849
#define HUGETLBFS_MAGIC 0x958458f6
#define ISOFS_SUPER_MAGIC 0x9660
#define JFFS2_SUPER_MAGIC 0x72b6
#define JFS_SUPER_MAGIC 0x3153464a
#define MINIX_SUPER_MAGIC 0x137f /* original minix FS */
#define MINIX_SUPER_MAGIC2 0x138f /* 30 char minix FS */
#define MINIX2_SUPER_MAGIC 0x2468 /* minix V2 FS */
#define MINIX2_SUPER_MAGIC2 0x2478 /* minix V2 FS, 30 char names */
#define MINIX3_SUPER_MAGIC 0x4d5a /* minix V3 FS, 60 char names */
#define MQUEUE_MAGIC 0x19800202 /* POSIX message queue FS */
#define MSDOS_SUPER_MAGIC 0x4d44
#define MTD_INODE_FS_MAGIC 0x11307854
#define NCP_SUPER_MAGIC 0x564c
#define NFS_SUPER_MAGIC 0x6969
#define NILFS_SUPER_MAGIC 0x3434
#define NSFS_MAGIC 0x6e736673
#define NTFS_SB_MAGIC 0x5346544e
#define OCFS2_SUPER_MAGIC 0x7461636f
#define OPENPROM_SUPER_MAGIC 0x9fa1
#define OVERLAYFS_SUPER_MAGIC 0x794c7630
#define PIPEFS_MAGIC 0x50495045
#define PROC_SUPER_MAGIC 0x9fa0 /* /proc FS */
#define PSTOREFS_MAGIC 0x6165676c
#define QNX4_SUPER_MAGIC 0x002f
#define QNX6_SUPER_MAGIC 0x68191122
#define RAMFS_MAGIC 0x858458f6
#define REISERFS_SUPER_MAGIC 0x52654973
#define ROMFS_MAGIC 0x7275
#define SECURITYFS_MAGIC 0x73636673
#define SELINUX_MAGIC 0xf97cff8c
#define SMACK_MAGIC 0x43415d53
#define SMB_SUPER_MAGIC 0x517b
#define SMB2_MAGIC_NUMBER 0xfe534d42
#define SOCKFS_MAGIC 0x534f434b
#define SQUASHFS_MAGIC 0x73717368
#define SYSFS_MAGIC 0x62656572
#define SYSV2_SUPER_MAGIC 0x012ff7b6
#define SYSV4_SUPER_MAGIC 0x012ff7b5
#define TMPFS_MAGIC 0x01021994
#define TRACEFS_MAGIC 0x74726163
#define UDF_SUPER_MAGIC 0x15013346
#define UFS_MAGIC 0x00011954
#define USBDEVICE_SUPER_MAGIC 0x9fa2
#define V9FS_MAGIC 0x01021997
#define VXFS_SUPER_MAGIC 0xa501fcf5
#define XENFS_SUPER_MAGIC 0xabba1974
#define XENIX_SUPER_MAGIC 0x012ff7b4
#define XFS_SUPER_MAGIC 0x58465342
#define _XIAFS_SUPER_MAGIC 0x012fd16d /* Linux 2.0 and earlier */

typedef uint64_t fsid_t;

#endif // _VFS_FSID_H