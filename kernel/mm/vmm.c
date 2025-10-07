/*********************************************************************************/
/* Module Name:  vmm.c */
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

#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/string.h>
#include <lib/align.h>
#include <stdbool.h>
#include <debug/log.h>

vctx_t *vinit(pagetable *pm, uint64_t start)
{
	vctx_t *ctx = (vctx_t *)(palloc(1) + boot_params->hhdm_offset);
	if (!ctx)
		return NULL;
	memset(ctx, 0, sizeof(vctx_t));
	ctx->root = (vregion_t *)(palloc(1) + boot_params->hhdm_offset);
	if (!ctx->root) {
		pfree(ctx, 1);
		return NULL;
	}

	ctx->pagemap = pm;
	ctx->root->start = start;
	ctx->root->pages = 0;
	return ctx;
}

void vdestroy(vctx_t *ctx)
{
	if (ctx->root == NULL || ctx->pagemap == NULL)
		return;

	vregion_t *region = ctx->root;
	while (region != NULL) {
		vregion_t *next = region->next;
		pfree(region, 1);
		region = next;
	}
	pfree(ctx, 1);
}

void *valloc(vctx_t *ctx, size_t pages, uint64_t flags)
{
	if (ctx == NULL || ctx->root == NULL || ctx->pagemap == NULL)
		return NULL;

	vregion_t *region = ctx->root;
	vregion_t *new = NULL;
	vregion_t *last = ctx->root;

	while (region) {
		if (region->next == NULL ||
			region->start + (region->pages * PAGE_SIZE) < region->next->start) {
			new = (vregion_t *)(palloc(1) + boot_params->hhdm_offset);
			if (!new)
				return NULL;

			memset(new, 0, sizeof(vregion_t));
			new->pages = pages;
			new->flags = VFLAGS_TO_PFLAGS(flags);
			new->start = region->start + (region->pages * PAGE_SIZE);
			new->next = region->next;
			new->prev = region;
			region->next = new;
			for (uint64_t i = 0; i < pages; i++) {
				uint64_t page = (uint64_t)palloc(1);
				if (page == 0)
					return NULL;

				map_page(ctx->pagemap, new->start + (i * PAGE_SIZE), page,
						 new->flags);
			}
			return (void *)new->start;
		}
		region = region->next;
	}

	new = (vregion_t *)(palloc(1) + boot_params->hhdm_offset);
	if (!new)
		return NULL;

	memset(new, 0, sizeof(vregion_t));
	last->next = new;
	new->prev = last;
	new->start = last->start + (last->pages * PAGE_SIZE);
	new->pages = pages;
	new->flags = VFLAGS_TO_PFLAGS(flags);
	new->next = NULL;

	for (uint64_t i = 0; i < pages; i++) {
		uint64_t page = (uint64_t)palloc(1);
		if (page == 0)
			return NULL;

		map_page(ctx->pagemap, new->start + (i * PAGE_SIZE), page, new->flags);
	}
	return (void *)new->start;
}

void *vallocat(vctx_t *ctx, size_t pages, uint64_t flags, uint64_t phys)
{
	if (ctx == NULL || ctx->root == NULL || ctx->pagemap == NULL)
		return NULL;

	vregion_t *region = ctx->root;
	vregion_t *new = NULL;
	vregion_t *last = ctx->root;

	phys = ALIGN_DOWN(phys, PAGE_SIZE);

	while (region) {
		if (region->next == NULL ||
			region->start + (pages * PAGE_SIZE) < region->next->start) {
			new = (vregion_t *)(palloc(1) + boot_params->hhdm_offset);
			if (!new)
				return NULL;

			memset(new, 0, sizeof(vregion_t));
			new->pages = pages;
			new->flags = VFLAGS_TO_PFLAGS(flags);
			new->start = region->start + (region->pages * PAGE_SIZE);
			new->next = region->next;
			new->prev = region;
			region->next = new;
			for (uint64_t i = 0; i < pages; i++) {
				uint64_t page = phys + (i * PAGE_SIZE);
				if (page == 0)
					return NULL;

				map_page(ctx->pagemap, new->start + (i * PAGE_SIZE), page,
						 new->flags);
			}
			return (void *)new->start;
		}
		region = region->next;
	}

	new = (vregion_t *)(palloc(1) + boot_params->hhdm_offset);
	if (!new)
		return NULL;

	memset(new, 0, sizeof(vregion_t));
	last->next = new;
	new->prev = last;
	new->start = last->start + (last->pages * PAGE_SIZE);
	new->pages = pages;
	new->flags = VFLAGS_TO_PFLAGS(flags);
	new->next = NULL;

	for (uint64_t i = 0; i < pages; i++) {
		uint64_t page = phys + (i * PAGE_SIZE);
		if (page == 0)
			return NULL;

		map_page(ctx->pagemap, new->start + (i * PAGE_SIZE), page, new->flags);
	}
	return (void *)new->start;
}

void *vadd(vctx_t *ctx, uint64_t vaddr, uint64_t paddr, size_t pages,
		   uint64_t flags)
{
	if (ctx == NULL || ctx->root == NULL || ctx->pagemap == NULL)
		return NULL;

	vaddr = ALIGN_DOWN(vaddr, PAGE_SIZE);
	paddr = ALIGN_DOWN(paddr, PAGE_SIZE);

	uint64_t vend = vaddr + pages * PAGE_SIZE;

	vregion_t *region = ctx->root;
	while (region) {
		uint64_t rstart = region->start;
		uint64_t rend = region->start + region->pages * PAGE_SIZE;

		if ((vaddr >= rstart && vaddr < rend) ||
			(vend > rstart && vend <= rend) ||
			(vaddr <= rstart && vend >= rend)) {
			warn("vadd: overlapping region at 0x%lx\n", vaddr);
			return NULL;
		}
		region = region->next;
	}

	vregion_t *new = (vregion_t *)(palloc(1) + boot_params->hhdm_offset);
	if (!new)
		return NULL;

	memset(new, 0, sizeof(vregion_t));
	new->start = vaddr;
	new->pages = pages;
	new->flags = VFLAGS_TO_PFLAGS(flags);

	new->next = ctx->root;
	if (ctx->root)
		ctx->root->prev = new;
	ctx->root = new;

	for (uint64_t i = 0; i < pages; i++) {
		uint64_t vpage = vaddr + (i * PAGE_SIZE);
		uint64_t ppage = paddr + (i * PAGE_SIZE);
		map_page(ctx->pagemap, vpage, ppage, new->flags);
	}

	return (void *)vaddr;
}

void vfree(vctx_t *ctx, void *ptr)
{
	if (ctx == NULL)
		return;

	vregion_t *region = ctx->root;
	while (region != NULL) {
		if (region->start == (uint64_t)ptr) {
			break;
		}
		region = region->next;
	}

	if (region == NULL)
		return;

	vregion_t *prev = region->prev;
	vregion_t *next = region->next;

	for (uint64_t i = 0; i < region->pages; i++) {
		uint64_t virt = region->start + (i * PAGE_SIZE);
		uint64_t phys = virt_to_phys(kernel_pm, virt);

		if (phys != 0) {
			pfree((void *)phys, 1);
			unmap_page(ctx->pagemap, virt);
		}
	}

	if (prev != NULL)
		prev->next = next;

	if (next != NULL)
		next->prev = prev;

	if (region == ctx->root)
		ctx->root = next;

	pfree(region, 1);
}

vregion_t *vget(vctx_t *ctx, uint64_t vaddr)
{
	if (ctx == NULL || ctx->root == NULL)
		return NULL;

	vregion_t *region = ctx->root;
	while (region != NULL) {
		uint64_t start = region->start;
		uint64_t end = start + (region->pages * PAGE_SIZE);

		if (vaddr >= start && vaddr < end)
			return region;

		region = region->next;
	}

	return NULL;
}