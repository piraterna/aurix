/*********************************************************************************/
/* Module Name:  vmm.h */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#ifndef _MM_VMM_H
#define _MM_VMM_H

#include <arch/mm/paging.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

pagetable *create_pagemap(void);
void destroy_pagemap(pagetable *pm);

bool paging_init(void);

void map_page(pagetable *pm, uintptr_t virt, uintptr_t phys, uint64_t flags);
void map_pages(pagetable *pm, uintptr_t virt, uintptr_t phys, size_t size,
			   uint64_t flags);

void unmap_page(pagetable *pm, uintptr_t virt);
void unmap_pages(pagetable *pm, uintptr_t virt, size_t size);

#ifndef VPM_MIN_ADDR
#define VPM_MIN_ADDR 0x1000
#endif // VPM_MIN_ADDR

#define VALLOC_NONE 0x0
#define VALLOC_READ (1 << 0)
#define VALLOC_WRITE (1 << 1)
#define VALLOC_EXEC (1 << 2)
#define VALLOC_USER (1 << 3)

#define VALLOC_RW (VALLOC_READ | VALLOC_WRITE)
#define VALLOC_RX (VALLOC_READ | VALLOC_EXEC)
#define VALLOC_RWX (VALLOC_READ | VALLOC_WRITE | VALLOC_EXEC)

#define VFLAGS_TO_PFLAGS(flags)                                    \
	(VMM_PRESENT | (((flags) & VALLOC_WRITE) ? VMM_WRITABLE : 0) | \
	 (((flags) & VALLOC_USER) ? VMM_USER : 0) |                    \
	 (((flags) & VALLOC_EXEC) ? 0 : VMM_NX))

typedef struct vregion {
	uint64_t start;
	uint64_t pages;
	uint64_t flags;
	struct vregion *next;
	struct vregion *prev;
} vregion_t;

typedef struct vctx {
	vregion_t *root;
	pagetable *pagemap;
	uint64_t start;
} vctx_t;

vctx_t *vinit(pagetable *pm, uint64_t start);
void vdestroy(vctx_t *ctx);
void *valloc(vctx_t *ctx, size_t pages, uint64_t flags);
void *vallocat(vctx_t *ctx, size_t pages, uint64_t flags, uint64_t phys);
void *vadd(vctx_t *ctx, uint64_t vaddr, uint64_t paddr, size_t pages,
		   uint64_t flags);
void vfree(vctx_t *ctx, void *ptr);

vregion_t *vget(vctx_t *ctx, uint64_t vaddr);
uintptr_t vget_phys(pagetable *pm, uintptr_t virt);

#endif /* _MM_VMM_H */