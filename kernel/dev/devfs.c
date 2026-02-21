/*********************************************************************************/
/* Module Name:  devfs.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

// this is just an example "devfs" for testing device/driver system for serial UART driver.
#include <dev/devfs.h>

#include <arch/cpu/cpu.h>
#include <arch/sys/irqlock.h>
#include <mm/heap.h>

#include <aurix.h>
#include <lib/string.h>

struct devfs_node {
	char *name;
	const struct chrdev_ops *ops;
	void *ctx;
	uint64_t owner_cr3;
	struct devfs_node *next;
};

static irqlock_t devfs_lock;
static struct devfs_node *devfs_list;

void devfs_init(void)
{
	irqlock_init(&devfs_lock);
	devfs_list = NULL;
	debug("devfs: init\n");
}

static struct devfs_node *devfs_find(const char *name)
{
	for (struct devfs_node *n = devfs_list; n; n = n->next) {
		if (n->name && name && strcmp(n->name, name) == 0)
			return n;
	}
	return NULL;
}

int devfs_register(const char *name, const struct chrdev_ops *ops, void *ctx)
{
	if (!name || !ops) {
		warn("devfs: invalid name=%p ops=%p\n", name, ops);
		return -1;
	}

	struct devfs_node *n = (struct devfs_node *)kmalloc(sizeof(*n));
	if (!n)
		return -1;
	memset(n, 0, sizeof(*n));

	size_t nlen = strlen(name);
	n->name = (char *)kmalloc(nlen + 1);
	if (!n->name) {
		kfree(n);
		return -1;
	}
	memcpy(n->name, name, nlen);
	n->name[nlen] = '\0';

	n->ops = ops;
	n->ctx = ctx;
	n->owner_cr3 = read_cr3();

	irqlock_acquire(&devfs_lock);
	if (devfs_find(name)) {
		irqlock_release(&devfs_lock);
		warn("devfs: duplicate node %s\n", name);
		kfree(n->name);
		kfree(n);
		return -1;
	}
	n->next = devfs_list;
	devfs_list = n;
	irqlock_release(&devfs_lock);

	return 0;
}

int devfs_read(const char *name, void *buf, size_t len)
{
	irqlock_acquire(&devfs_lock);
	struct devfs_node *n = devfs_find(name);
	const struct chrdev_ops *ops = n ? n->ops : NULL;
	void *ctx = n ? n->ctx : NULL;
	irqlock_release(&devfs_lock);

	if (!ops || !ops->read) {
		debug("devfs: read from missing/unreadable node %s\n",
			  name ? name : "(null)");
		return -1;
	}
	return ops->read(ctx, buf, len);
}

int devfs_write(const char *name, const void *buf, size_t len)
{
	irqlock_acquire(&devfs_lock);
	struct devfs_node *n = devfs_find(name);
	const struct chrdev_ops *ops = n ? n->ops : NULL;
	void *ctx = n ? n->ctx : NULL;
	uint64_t owner_cr3 = n ? n->owner_cr3 : 0;
	irqlock_release(&devfs_lock);

	if (!ops || !ops->write) {
		debug("devfs: write to missing/unwritable node %s\n",
			  name ? name : "(null)");
		return -1;
	}

	void *kbuf = NULL;
	if (len) {
		kbuf = kmalloc(len);
		if (!kbuf)
			return -1;
		memcpy(kbuf, buf, len);
	}

	uint8_t irq_state = save_if();
	cpu_disable_interrupts();
	uint64_t prev_cr3 = read_cr3();
	if (owner_cr3)
		write_cr3(owner_cr3);
	int rc = ops->write(ctx, kbuf, len);
	write_cr3(prev_cr3);
	restore_if(irq_state);

	if (kbuf)
		kfree(kbuf);
	return rc;
}

int devfs_exists(const char *name)
{
	if (!name)
		return 0;
	int ok = 0;

	irqlock_acquire(&devfs_lock);
	if (devfs_find(name))
		ok = 1;
	irqlock_release(&devfs_lock);

	return ok;
}
