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

#include <boot/axprot.h>
#include <debug/log.h>
#include <dev/builtin/fb.h>
#include <dev/device.h>
#include <dev/driver.h>
#include <lib/string.h>
#include <aurix.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <aurix.h>

int fb_write(struct device *dev, const void *buf, size_t len,
				 size_t offset);
int fb_read(struct device *dev, void *buf, size_t len, size_t offset);
int fb_ioctl(struct device *dev, uint64_t cmd, void *arg);

uintptr_t framebuf_addr = 0;

struct device_ops fb_ops = {
	.open = NULL,
	.close = NULL,
	.read = fb_read,
	.write = fb_write,
	.ioctl = fb_ioctl,
	.poll = NULL,
};

struct device fb_dev = {
	.name = "fb",
	.class_name = "fb",
	.dev_node_path = "fb",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &fb_ops,
	.next = NULL,
};

int fb_write(struct device *dev, const void *buf, size_t len, size_t offset)
{
	(void)dev;
	memcpy((void *)((uint8_t *)framebuf_addr + offset), buf, len * 4);
	return (int)len;
}

int fb_read(struct device *dev, void *buf, size_t len, size_t offset)
{
	(void)dev;
	if (!buf)
		return -1;
	if (len == 0)
		return 0;

	memcpy(buf, (void *)((uint8_t *)framebuf_addr + offset), len);

	return len;
}

int fb_ioctl(struct device *dev, uint64_t cmd, void *arg)
{
	(void)dev;
	(void)cmd;
	(void)arg;
	warn("fb: ioctl not implemented\n");
	return -1;
}

void fb_init()
{
	framebuf_addr = boot_params->framebuffer.addr;
	if (framebuf_addr == 0) {
		error("fb: address is null!\n");
		return;
	}
	device_register(&fb_dev);
	driver_bind_all();
	trace("fb: initialized\n");
}
