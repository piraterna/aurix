#ifndef _SYS_ERRNO_H
#define _SYS_ERRNO_H

#define EFAULT 1
#define ENOENT 2
#define EBADF 3
#define EIO 4
#define ENOSYS 5

#define ERRNO_NAME(err)           \
	((err) == EFAULT ? "EFAULT" : \
	 (err) == ENOENT ? "ENOENT" : \
	 (err) == EBADF	 ? "EBADF" :  \
	 (err) == EIO	 ? "EIO" :    \
	 (err) == ENOSYS ? "ENOSYS" : \
					   "EUNKNOWN")

#endif // _SYS_ERRNO_H
