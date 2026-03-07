#ifndef _IPC_PIPE_H
#define _IPC_PIPE_H

#include <vfs/fileio.h>
#include <sys/spinlock.h>
#include <stdint.h>
#include <stddef.h>

#define PIPE_BUFFER_SIZE 4096

struct pipe {
	uint8_t buffer[PIPE_BUFFER_SIZE];
	size_t read_pos;
	size_t write_pos;
	size_t used;

	int readers;
	int writers;

	spinlock_t lock;
};

int pipe(struct fileio *fds[2]);
int pipe_read(struct fileio *fio, void *buf, size_t *size);
int pipe_write(struct fileio *fio, const void *buf, size_t *size);
int pipe_close(struct fileio *fio);

#endif /* _IPC_PIPE_H */