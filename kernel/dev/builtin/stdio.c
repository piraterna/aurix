/*********************************************************************************/
/* Module Name:  stdio.c */
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

#include <dev/builtin/stdio.h>
#include <dev/driver.h>
#include <dev/device.h>
#include <util/kprintf.h>
#include <debug/log.h>
#include <lib/string.h>

int stdout_write(struct device *dev, const void *buf, size_t len,
				 size_t offset);
int stdio_probe(struct device *dev);

struct device_ops stdout_ops = {
	.open = NULL,
	.close = NULL,
	.read = NULL,
	.write = stdout_write,
	.ioctl = NULL,
	.poll = NULL,
};

// TODO: proper stdout with ttys and shit
struct device stdout = {
	.name = "stdout",
	.class_name = "stdio",
	.dev_node_path = "/stdout",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &stdout_ops,
	.next = NULL,
};

struct driver stdio_driver = {
	.name = "stdio",
	.class_name = "stdio",
	.probe = stdio_probe,
	.remove = NULL,
};

int stdout_write(struct device *dev, const void *buf, size_t len, size_t offset)
{
	(void)dev;
	(void)offset;

	kprintf("%.*s", (int)len, (const char *)buf);

	return (int)len;
}

int stdio_probe(struct device *dev)
{
	if (strcmp(dev->class_name, "stdio") != 0) {
		return -1;
	}

	dev->ops = &stdout_ops;
	dev->driver_data = NULL;
	trace("stdio: bound to device %s\n", dev->name ? dev->name : "(unnamed)");
	return 0;
}

void stdio_init()
{
	device_register(&stdout);
	driver_register(&stdio_driver);
	driver_bind_all();
	trace("stdio: initialized\n");
}