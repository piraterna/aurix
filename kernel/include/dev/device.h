/*********************************************************************************/
/* Module Name:  device.h */
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

#ifndef _DEV_DEVICE_H
#define _DEV_DEVICE_H

#include <stdint.h>

struct driver;
struct device;

struct device_ops {
	int (*open)(struct device *dev);
	int (*close)(struct device *dev);
	int (*read)(struct device *dev, void *buf, uint64_t len);
	int (*write)(struct device *dev, const void *buf, uint64_t len);
	int (*ioctl)(struct device *dev, uint64_t cmd, void *arg);
	int (*poll)(struct device *dev);
};

struct device {
	const char *name;
	const char *class_name;

	const char *dev_node_path;

	void *driver_data;

	struct driver *bound_driver;
	struct device_ops *ops;

	struct device *next;
};

int device_get_count(void);
struct device *device_get_list(void);

#endif