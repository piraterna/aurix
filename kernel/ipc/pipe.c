/*********************************************************************************/
/* Module Name:  pipe.c */
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

#include <ipc/pipe.h>
#include <vfs/fileio.h>
#include <mm/heap.h>
#include <lib/string.h>

static inline size_t pipe_space(struct pipe *p)
{
	return PIPE_BUFFER_SIZE - p->used;
}

static inline size_t pipe_contig_read(struct pipe *p)
{
	size_t end = PIPE_BUFFER_SIZE - p->read_pos;
	return (p->used < end) ? p->used : end;
}

static inline size_t pipe_contig_write(struct pipe *p)
{
	size_t end = PIPE_BUFFER_SIZE - p->write_pos;
	return (pipe_space(p) < end) ? pipe_space(p) : end;
}

int pipe(struct fileio *fds[2])
{
	struct pipe *p = kmalloc(sizeof(struct pipe));
	if (!p)
		return -1;
	memset(p, 0, sizeof(struct pipe));
	spinlock_acquire(&p->lock);
	p->readers = 1;
	p->writers = 1;

	struct fileio *rd = fio_create();
	struct fileio *wr = fio_create();
	if (!rd || !wr) {
		if (rd)
			kfree(rd);
		if (wr)
			kfree(wr);
		kfree(p);
		return -1;
	}

	rd->flags = PIPE_READ_END;
	rd->offset = 0;
	rd->size = 0;
	rd->private = p;

	wr->flags = PIPE_WRITE_END;
	wr->offset = 0;
	wr->size = 0;
	wr->private = p;

	fds[0] = rd;
	fds[1] = wr;
	spinlock_release(&p->lock);
	return 0;
}

int pipe_read(struct fileio *fio, void *buf, size_t *size)
{
	struct pipe *p = (struct pipe *)fio->private;
	size_t requested = *size;
	size_t read_bytes = 0;

	spinlock_acquire(&p->lock);
	while (read_bytes == 0) {
		if (p->used > 0) {
			size_t to_read = pipe_contig_read(p);
			if (to_read > requested)
				to_read = requested;

			memcpy(buf, &p->buffer[p->read_pos], to_read);
			p->read_pos = (p->read_pos + to_read) % PIPE_BUFFER_SIZE;
			p->used -= to_read;
			read_bytes = to_read;
		} else {
			if (p->writers == 0) {
				spinlock_release(&p->lock);
				*size = 0;
				return 0;
			}

			spinlock_release(&p->lock);
			*size = 0;
			return 0;
		}
	}
	spinlock_release(&p->lock);

	*size = read_bytes;
	return 0;
}

int pipe_write(struct fileio *fio, const void *buf, size_t *size)
{
	struct pipe *p = (struct pipe *)fio->private;
	size_t requested = *size;
	size_t written = 0;

	spinlock_acquire(&p->lock);
	while (written < requested) {
		if (pipe_space(p) > 0) {
			size_t to_write = pipe_contig_write(p);
			if (to_write > requested - written)
				to_write = requested - written;

			memcpy(&p->buffer[p->write_pos], (const uint8_t *)buf + written,
				   to_write);
			p->write_pos = (p->write_pos + to_write) % PIPE_BUFFER_SIZE;
			p->used += to_write;
			written += to_write;
		} else {
			if (p->readers == 0) {
				spinlock_release(&p->lock);
				return -1;
			}

			break;
		}
	}
	spinlock_release(&p->lock);

	*size = written;
	return 0;
}

int pipe_close(struct fileio *fio)
{
	struct pipe *p = (struct pipe *)fio->private;
	spinlock_acquire(&p->lock);

	if (fio->flags & PIPE_READ_END)
		p->readers--;
	if (fio->flags & PIPE_WRITE_END)
		p->writers--;

	bool destroy = (p->readers == 0 && p->writers == 0);
	spinlock_release(&p->lock);

	kfree(fio);
	if (destroy)
		kfree(p);

	return 0;
}