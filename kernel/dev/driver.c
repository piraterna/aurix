/*********************************************************************************/
/* Module Name:  driver.c */
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

#include <dev/driver.h>

#include <arch/sys/irqlock.h>
#include <arch/cpu/cpu.h>
#include <mm/heap.h>

#include <aurix.h>
#include <lib/string.h>

struct dev_node {
	struct device dev;
	struct dev_node *next;
};

struct drv_node {
	struct driver drv;
	uint64_t owner_cr3;
	int bound_count;
	struct drv_node *next;
};

static irqlock_t dev_lock;
static struct dev_node *dev_list;

static irqlock_t drv_lock;
static struct drv_node *drv_list;

static char *kstrdup(const char *s)
{
	if (!s)
		return NULL;
	size_t len = strlen(s);
	char *out = (char *)kmalloc(len + 1);
	if (!out)
		return NULL;
	memcpy(out, s, len);
	out[len] = '\0';
	return out;
}

static int class_match(const char *a, const char *b)
{
	if (!a || !b)
		return 0;
	return strcmp(a, b) == 0;
}

void driver_core_init(void)
{
	irqlock_init(&dev_lock);
	irqlock_init(&drv_lock);
	debug("core init\n");
}

int device_register(struct device *dev)
{
	if (!dev || !dev->name) {
		warn("invalid dev=%p name=%p\n", dev, dev ? (void *)dev->name : NULL);
		return -1;
	}

	struct dev_node *n = (struct dev_node *)kmalloc(sizeof(*n));
	if (!n) {
		error("kmalloc failed for %s\n", dev->name);
		return -1;
	}
	memset(n, 0, sizeof(*n));

	n->dev.name = kstrdup(dev->name);
	n->dev.class_name = kstrdup(dev->class_name);
	n->dev.driver_data = NULL;
	n->dev.bound_driver = NULL;
	if (!n->dev.name) {
		error("name OOM\n");
		kfree((void *)n->dev.class_name);
		kfree(n);
		return -1;
	}
	n->next = NULL;

	irqlock_acquire(&dev_lock);
	for (struct dev_node *it = dev_list; it; it = it->next) {
		if (it->dev.name && strcmp(it->dev.name, dev->name) == 0) {
			warn("duplicate name=%s class=%s\n", dev->name,
				 dev->class_name ? dev->class_name : "(none)");
			irqlock_release(&dev_lock);
			kfree((void *)n->dev.name);
			kfree((void *)n->dev.class_name);
			kfree(n);
			return -1;
		}
	}
	n->next = dev_list;
	dev_list = n;
	irqlock_release(&dev_lock);

	debug("+ %s class=%s\n", n->dev.name,
		  n->dev.class_name ? n->dev.class_name : "(none)");

	return 0;
}

int driver_register(struct driver *drv)
{
	if (!drv || !drv->name || !drv->probe) {
		warn("invalid drv=%p name=%p probe=%p\n", drv,
			 drv ? (void *)drv->name : NULL, drv ? (void *)drv->probe : NULL);
		return -1;
	}

	struct drv_node *n = (struct drv_node *)kmalloc(sizeof(*n));
	if (!n) {
		error("kmalloc failed for %s\n", drv->name);
		return -1;
	}
	memset(n, 0, sizeof(*n));

	n->drv.name = kstrdup(drv->name);
	n->drv.class_name = kstrdup(drv->class_name);
	n->drv.probe = drv->probe;
	n->drv.remove = drv->remove;
	n->owner_cr3 = read_cr3();
	if (!n->drv.name) {
		error("name OOM\n");
		kfree((void *)n->drv.class_name);
		kfree(n);
		return -1;
	}
	n->next = NULL;

	irqlock_acquire(&drv_lock);
	for (struct drv_node *it = drv_list; it; it = it->next) {
		if (it->drv.name && strcmp(it->drv.name, drv->name) == 0) {
			warn("duplicate name=%s class=%s\n", drv->name,
				 drv->class_name ? drv->class_name : "(none)");
			irqlock_release(&drv_lock);
			kfree((void *)n->drv.name);
			kfree((void *)n->drv.class_name);
			kfree(n);
			return -1;
		}
	}
	n->next = drv_list;
	drv_list = n;
	irqlock_release(&drv_lock);

	debug("+ %s class=%s\n", n->drv.name,
		  n->drv.class_name ? n->drv.class_name : "(none)");

	return 0;
}

int driver_bind_all(void)
{
	int bound = 0;
	debug("binding devices\n");

	irqlock_acquire(&drv_lock);
	struct drv_node *d = drv_list;
	irqlock_release(&drv_lock);

	for (; d; d = d->next) {
		struct driver *drv = &d->drv;

		irqlock_acquire(&dev_lock);
		struct dev_node *n = dev_list;
		irqlock_release(&dev_lock);

		for (; n; n = n->next) {
			struct device *dev = &n->dev;
			if (dev->bound_driver)
				continue;
			if (!class_match(drv->class_name, dev->class_name))
				continue;

			uint8_t irq_state = save_if();
			cpu_disable_interrupts();
			uint64_t prev_cr3 = read_cr3();
			if (d->owner_cr3)
				write_cr3(d->owner_cr3);
			int rc = drv->probe(dev);
			write_cr3(prev_cr3);
			restore_if(irq_state);

			if (rc == 0) {
				dev->bound_driver = drv;
				d->bound_count++;
				debug("bind %s -> %s (class=%s)\n", drv->name, dev->name,
					  dev->class_name ? dev->class_name : "(none)");
				bound++;
			}
		}
	}

	success("bound %d pair(s)\n", bound);
	return bound;
}

int driver_is_ready(const char *driver_name)
{
	if (!driver_name)
		return 0;

	int ok = 0;
	irqlock_acquire(&drv_lock);
	for (struct drv_node *n = drv_list; n; n = n->next) {
		if (n->drv.name && strcmp(n->drv.name, driver_name) == 0) {
			ok = (n->bound_count > 0);
			break;
		}
	}
	irqlock_release(&drv_lock);
	return ok;
}

int driver_exists(const char *driver_name)
{
	if (!driver_name)
		return 0;

	int ok = 0;
	irqlock_acquire(&drv_lock);
	for (struct drv_node *n = drv_list; n; n = n->next) {
		if (n->drv.name && strcmp(n->drv.name, driver_name) == 0) {
			ok = 1;
			break;
		}
	}
	irqlock_release(&drv_lock);
	return ok;
}

int ax_driver_exists(const char *driver_name)
{
	return driver_exists(driver_name);
}

int ax_driver_is_ready(const char *driver_name)
{
	return driver_is_ready(driver_name);
}

int ax_device_register(struct device *dev)
{
	return device_register(dev);
}

int ax_driver_register(struct driver *drv)
{
	return driver_register(drv);
}

int ax_driver_bind_all(void)
{
	return driver_bind_all();
}
