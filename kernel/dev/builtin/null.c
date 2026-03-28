/*********************************************************************************/
/* Module Name:  null.c */
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

#include <dev/builtin/null.h>
#include <dev/driver.h>

static int null_read(struct device *dev, void *buf, size_t len, size_t offset)
{
	(void)dev;
	(void)buf;
	(void)len;
	(void)offset;

	return 0;
}

static int null_write(struct device *dev, const void *buf, size_t len,
					  size_t offset)
{
	(void)dev;
	(void)buf;
	(void)offset;

	return (int)len;
}

static int null_ioctl(struct device *dev, uint64_t cmd, void *arg)
{
	(void)dev;
	(void)cmd;
	(void)arg;

	return -1;
}

static int null_poll(struct device *dev)
{
	(void)dev;
	return 1;
}

static struct device_ops null_ops = {
	.open = NULL,
	.close = NULL,
	.read = null_read,
	.write = null_write,
	.ioctl = null_ioctl,
	.poll = null_poll,
};

static struct device null_dev = {
	.name = "null",
	.class_name = "misc",
	.dev_node_path = "null",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &null_ops,
	.next = NULL,
};

void null_init(void)
{
	device_register(&null_dev);
}
