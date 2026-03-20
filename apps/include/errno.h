#ifndef _ERRNO_H
#define _ERRNO_H

#define EFAULT 1
#define ENOENT 2
#define EBADF 3
#define EIO 4
#define ENOSYS 5

static inline int *__errno_location(void)
{
	static int app_errno = 0;
	return &app_errno;
}

#define errno (*__errno_location())

#endif // _ERRNO_H
