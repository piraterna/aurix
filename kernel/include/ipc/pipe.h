/*********************************************************************************/
/* Module Name:  pipe.h */
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