/*********************************************************************************/
/* Module Name:  driver.c */
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

#include <dev/driver.h>
#include <dev/device.h>
#include <fs/devfs.h>

#include <arch/sys/irqlock.h>
#include <arch/cpu/cpu.h>
#include <mm/heap.h>

#include <aurix.h>
#include <lib/string.h>
#include <debug/log.h>

struct drv_node {
	struct driver drv;
	uint64_t owner_cr3;
	int bound_count;
	struct drv_node *next;
};

static irqlock_t dev_lock;
static struct device *dev_list;
static int dev_count;

static irqlock_t drv_lock;
static struct drv_node *drv_list;

static irqlock_t dev_lock;
struct device *device_list[MAX_DEVICES];
int device_count = 0;

static struct devfs *devfs = NULL;

static char *kstrdup(const char *s)
{
	if (!s)
		return NULL;

	size_t len = strlen(s);
	char *out = kmalloc(len + 1);
	if (!out)
		return NULL;

	memcpy(out, s, len);
	out[len] = '\0';
	return out;
}

static void devfs_publish_device(struct device *dev)
{
	if (!dev || !dev->dev_node_path)
		return;

	if (!global_devfs)
		return;

	char *path = strdup(dev->dev_node_path);
	char *saveptr;
	char *token;

	struct devfs_node *current = global_devfs->root_node;

	bool ends_with_slash = path[strlen(path) - 1] == '/';

	token = strtok_r(path, "/", &saveptr);

	while (token) {
		char *next = strtok_r(NULL, "/", &saveptr);
		bool is_last = (next == NULL);

		if (is_last && !ends_with_slash) {
			struct devfs_node *node = devfs_create_node(DEVFS_TYPE_CHAR);
			node->name = strdup(token);
			node->device = dev;

			if (devfs_append_child(current, node) != 0) {
				error("driver: failed to publish device\n");
				kfree(path);
				return;
			}

			debug("driver: published device %s -> %s\n", dev->name,
				  dev->dev_node_path);
		} else {
			struct devfs_node *dir = devfs_find_child(current, token);

			if (!dir) {
				dir = devfs_create_node(DEVFS_TYPE_DIR);
				dir->name = strdup(token);
				devfs_append_child(current, dir);
			}

			current = dir;

			if (is_last && ends_with_slash) {
				warn(
					"driver: device path ends with '/', treating as directory: %s\n",
					dev->dev_node_path);
			}
		}

		token = next;
	}

	kfree(path);
}

static int class_match(const char *a, const char *b)
{
	if (!a || !b)
		return 0;
	return strcmp(a, b) == 0;
}

void driver_core_init(struct devfs *fs)
{
	irqlock_init(&dev_lock);
	irqlock_init(&drv_lock);

	dev_list = NULL;
	drv_list = NULL;
	dev_count = 0;
	devfs = fs;

	debug("driver core init\n");
}

int device_register(struct device *dev)
{
	if (!dev || !dev->name) {
		warn("invalid device or name\n");
		return -1;
	}

	irqlock_acquire(&dev_lock);

	for (int i = 0; i < device_count; i++) {
		if (strcmp(device_list[i]->name, dev->name) == 0) {
			warn("duplicate device name=%s\n", dev->name);
			irqlock_release(&dev_lock);
			return -1;
		}
	}

	if (device_count >= MAX_DEVICES) {
		warn("device list full, cannot register %s\n", dev->name);
		irqlock_release(&dev_lock);
		return -1;
	}

	struct device *n = kmalloc(sizeof(*n));
	if (!n) {
		error("kmalloc failed for device\n");
		irqlock_release(&dev_lock);
		return -1;
	}

	memset(n, 0, sizeof(*n));
	n->name = kstrdup(dev->name);
	n->class_name = kstrdup(dev->class_name);
	n->dev_node_path = kstrdup(dev->dev_node_path);
	n->driver_data = dev->driver_data;
	n->ops = dev->ops;
	n->bound_driver = NULL;

	if (!n->name) {
		kfree((void *)n->class_name);
		kfree((void *)n->dev_node_path);
		kfree(n);
		irqlock_release(&dev_lock);
		return -1;
	}

	device_list[device_count++] = n;

	irqlock_release(&dev_lock);

	devfs_publish_device(n);

	debug("device registered: %s class=%s\n", n->name,
		  n->class_name ? n->class_name : "(none)");

	return 0;
}

int driver_register(struct driver *drv)
{
	if (!drv || !drv->name || !drv->probe) {
		warn("invalid driver or probe missing\n");
		return -1;
	}

	struct drv_node *n = kmalloc(sizeof(*n));
	if (!n) {
		error("kmalloc failed for driver\n");
		return -1;
	}

	memset(n, 0, sizeof(*n));
	n->drv.name = kstrdup(drv->name);
	n->drv.class_name = kstrdup(drv->class_name);
	n->drv.probe = drv->probe;
	n->drv.remove = drv->remove;

	n->owner_cr3 = read_cr3();

	if (!n->drv.name) {
		kfree((void *)n->drv.class_name);
		kfree(n);
		return -1;
	}

	irqlock_acquire(&drv_lock);

	for (struct drv_node *it = drv_list; it; it = it->next) {
		if (strcmp(it->drv.name, drv->name) == 0) {
			irqlock_release(&drv_lock);

			warn("duplicate driver name=%s\n", drv->name);

			kfree((void *)n->drv.name);
			kfree((void *)n->drv.class_name);
			kfree(n);
			return -1;
		}
	}

	n->next = drv_list;
	drv_list = n;

	irqlock_release(&drv_lock);

	debug("driver registered: %s class=%s\n", n->drv.name,
		  n->drv.class_name ? n->drv.class_name : "(none)");

	return 0;
}

int driver_bind_all(void)
{
	int total_bound = 0;
	debug("binding drivers to devices\n");

	for (struct drv_node *d = drv_list; d; d = d->next) {
		struct driver *drv = &d->drv;
		int driver_bound = 0;

		irqlock_acquire(&dev_lock);
		for (int i = 0; i < device_count; i++) {
			struct device *dev = device_list[i];

			if (dev->bound_driver)
				continue;

			if (!class_match(drv->class_name, dev->class_name))
				continue;

			uint8_t irq_state = save_if();
			cpu_disable_interrupts();

			uint64_t prev_cr3 = read_cr3();
			if (d->owner_cr3)
				write_cr3(d->owner_cr3);

			int rc = drv->probe ? drv->probe(dev) : -1;

			write_cr3(prev_cr3);
			restore_if(irq_state);

			if (rc == 0) {
				dev->bound_driver = drv;
				d->bound_count++;
				driver_bound++;
				success("bound %s -> %s (class=%s)\n", drv->name, dev->name,
						dev->class_name ? dev->class_name : "(none)");
			}
		}
		irqlock_release(&dev_lock);

		if (driver_bound == 0) {
			warn("driver %s: no devices bound\n", drv->name);
		}
	}

	return total_bound;
}

int device_get_count(void)
{
	int count;
	irqlock_acquire(&dev_lock);
	count = device_count;
	irqlock_release(&dev_lock);
	return count;
}
