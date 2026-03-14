/*********************************************************************************/
/* Module Name:  module.h */
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

#ifndef _LOADER_MODULE_H
#define _LOADER_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/sched.h>

bool module_load(void *addr, uint32_t size);

bool module_lookup_image(uintptr_t addr, char **elf_out,
						 uintptr_t *load_base_out, uintptr_t *link_base_out);

struct module_info_node {
	struct module_info_node *next;

	pcb *proc;

	const char *name;
	const char *desc;
	const char *author;

	int (*init)(void);
	void (*exit)(void);

	uintptr_t load_base;
};

struct module_info_node *module_get_list(void);

#endif /* _LOADER_MODULE_H */
