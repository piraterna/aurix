/*********************************************************************************/
/* Module Name:  driver.h */
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

#ifndef _DEV_DRIVER_H
#define _DEV_DRIVER_H

#include <dev/device.h>

typedef int (*driver_probe_fn)(struct device *dev);
typedef void (*driver_remove_fn)(struct device *dev);

struct driver {
	const char *name;
	const char *class_name;
	driver_probe_fn probe;
	driver_remove_fn remove;
};

void driver_core_init(void);
int driver_register(struct driver *drv);
int driver_bind_all(void);
int driver_is_ready(const char *driver_name);

int driver_exists(const char *driver_name);

#endif
