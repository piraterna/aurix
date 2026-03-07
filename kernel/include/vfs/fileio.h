#ifndef _VFS_FILEIO_H
#define _VFS_FILEIO_H

#include <stdint.h>
#include <stddef.h>

enum fseek { SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2 };

enum fcntl_cmd {
	F_GETFL = 0,
	F_SETFL = 1,
};

#define O_APPEND (1 << 0)
#define O_CREATE (1 << 1)

#define PIPE_READ_END (1 << 2)
#define PIPE_WRITE_END (1 << 3)

#define SPECIAL_FILE_TYPE_PIPE (1 << 4)
#define SPECIAL_FILE_TYPE_DEVICE (1 << 5)

struct fileio {
	void *buf_start;
	size_t size;

	size_t flags;
	size_t offset;

	void *private;
};

struct fileio *fio_create();

struct fileio *open(char *path, int flags);
size_t read(struct fileio *file, size_t size, void *out);
int write(struct fileio *file, void *buf, size_t size);
int close(struct fileio *file);

size_t seek(struct fileio *file, size_t offset, uint8_t whence);

size_t fcntl(struct fileio *file, uint8_t cmd, void *arg);

#endif /* _VFS_FILEIO_H */