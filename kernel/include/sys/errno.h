#ifndef _SYS_ERRNO_H
#define _SYS_ERRNO_H

#define EFAULT 1
#define ENOENT 2
#define EBADF 3
#define EIO 4
#define ENOSYS 5
#define EINVAL 6
#define ENOMEM 7
#define ENOTTY 8

#define ERRNO_NAME(err)           \
	((err) == EFAULT ? "EFAULT" : \
	 (err) == ENOENT ? "ENOENT" : \
	 (err) == EBADF	 ? "EBADF" :  \
	 (err) == EIO	 ? "EIO" :    \
	 (err) == ENOSYS ? "ENOSYS" : \
	 (err) == EINVAL ? "EINVAL" : \
	 (err) == ENOMEM ? "ENOMEM" : \
	 (err) == ENOTTY ? "ENOTTY" : \
					   "EUNKNOWN")

#endif // _SYS_ERRNO_H
