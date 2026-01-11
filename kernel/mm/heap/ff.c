/*********************************************************************************/
/* Module Name:  ff.c                                                             */
/* Project:      AurixOS                                                          */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                              */
/*                                                                               */
/* This source is subject to the MIT License.                                     */
/* See License.txt in the root of this repository.                                */
/* All other rights reserved.                                                     */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR      */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <mm/heap.h>
#include <mm/heap/ff.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <lib/align.h>
#include <aurix.h>
#include <string.h>
#include <debug/log.h>

#define ALIGNMENT PAGE_SIZE
#define CANARY_SIZE sizeof(uint64_t)
#define CANARY_VALUE 0xdeadbeefdeadbeefULL
#define CHECK_MAGIC 0xfeedfacefeedfaceULL

static void *pool = NULL;
static size_t pool_pages = 0;
static block_t *freelist = NULL;
static vctx_t *heap_ctx = NULL;

static size_t compute_check(const block_t *b)
{
	return (uintptr_t)b->prev ^ (uintptr_t)b->next ^ b->size ^ b->user_size ^
		   b->alloc_size ^ CHECK_MAGIC;
}

static void set_check(block_t *b)
{
	b->check = compute_check(b);
}

static int validate(const block_t *b)
{
	if (!b)
		return 0;

	if (b->check != compute_check(b)) {
		critical("HEAP CORRUPTION: block header %p invalid\n", (void *)b);
		return 0;
	}

	if ((uint8_t *)b + sizeof(block_t) + b->size >
		(uint8_t *)pool + pool_pages * PAGE_SIZE) {
		critical("HEAP BLOCK OUT OF BOUNDS: %p\n", (void *)b);
		return 0;
	}

	return 1;
}

static uint64_t *leading_canary_ptr(const block_t *b)
{
	return (uint64_t *)((uint8_t *)b + sizeof(block_t));
}

static uint64_t *trailing_canary_ptr(const block_t *b)
{
	return (uint64_t *)((uint8_t *)b + sizeof(block_t) + b->alloc_size -
						CANARY_SIZE);
}

static void write_canaries(block_t *b)
{
	*leading_canary_ptr(b) = CANARY_VALUE;
	*trailing_canary_ptr(b) = CANARY_VALUE;
}

static int check_canaries(const block_t *b)
{
	if (*leading_canary_ptr(b) != CANARY_VALUE) {
		critical("HEAP OVERFLOW: leading canary corrupted (%p)\n", (void *)b);
		return 0;
	}
	if (*trailing_canary_ptr(b) != CANARY_VALUE) {
		critical("HEAP OVERFLOW: trailing canary corrupted (%p)\n", (void *)b);
		return 0;
	}
	return 1;
}

static void freelist_insert(block_t *b)
{
	block_t *cur = freelist;
	block_t *prev = NULL;

	while (cur && cur < b) {
		if (!validate(cur))
			return;
		prev = cur;
		cur = cur->next;
	}

	b->prev = prev;
	b->next = cur;
	set_check(b);

	if (prev) {
		prev->next = b;
		set_check(prev);
	} else {
		freelist = b;
	}

	if (cur) {
		cur->prev = b;
		set_check(cur);
	}
}

static void freelist_remove(block_t *b)
{
	if (b->prev) {
		b->prev->next = b->next;
		set_check(b->prev);
	} else {
		freelist = b->next;
	}

	if (b->next) {
		b->next->prev = b->prev;
		set_check(b->next);
	}

	b->prev = b->next = NULL;
}

static int grow_heap(vctx_t *ctx, size_t needed_pages)
{
	(void)needed_pages;
	size_t add_pages = pool_pages;
	void *mem = valloc(ctx, add_pages, VALLOC_RW);
	if (!mem) {
		error("Heap growth failed (%zu pages)\n", add_pages);
		return 0;
	}

	debug("Heap growing: +%zu pages (old=%zu new=%zu)\n", add_pages, pool_pages,
		  pool_pages + add_pages);

	block_t *b = (block_t *)mem;
	b->size = ALIGN_DOWN(add_pages * PAGE_SIZE - sizeof(block_t), ALIGNMENT);
	b->user_size = 0;
	b->alloc_size = 0;
	b->prev = b->next = NULL;
	set_check(b);

	freelist_insert(b);

	if (b->next &&
		(uint8_t *)b + sizeof(block_t) + b->size == (uint8_t *)b->next) {
		block_t *n = b->next;
		if (validate(n)) {
			b->size += sizeof(block_t) + n->size;
			freelist_remove(n);
			set_check(b);
		}
	}

	if (b->prev &&
		(uint8_t *)b->prev + sizeof(block_t) + b->prev->size == (uint8_t *)b) {
		block_t *p = b->prev;
		if (validate(p)) {
			p->size += sizeof(block_t) + b->size;
			freelist_remove(b);
			set_check(p);
		}
	}

	pool_pages += add_pages;
	return 1;
}

void heap_init(vctx_t *ctx)
{
	heap_ctx = ctx;

	pool = valloc(ctx, FF_POOL_SIZE, VALLOC_RW);
	if (!pool) {
		critical("Failed to allocate initial heap\n");
		for (;;)
			;
	}

	pool_pages = FF_POOL_SIZE;

	block_t *b = (block_t *)pool;
	b->size = ALIGN_DOWN(FF_POOL_SIZE * PAGE_SIZE - sizeof(block_t), ALIGNMENT);
	b->user_size = 0;
	b->alloc_size = 0;
	b->prev = b->next = NULL;
	set_check(b);

	freelist = b;
}

void *kmalloc(size_t size)
{
	if (size == 0)
		return NULL;

	size_t user_sz = ALIGN_UP(size, ALIGNMENT);
	size_t payload = user_sz + 2 * CANARY_SIZE;
	size_t effective = ALIGN_UP(payload, ALIGNMENT);

	block_t *cur = freelist;
	block_t *chosen = NULL;

	while (cur) {
		if (!validate(cur))
			return NULL;

		if (cur->size >= effective) {
			chosen = cur;
			break;
		}
		cur = cur->next;
	}

	if (!chosen) {
		warn("No suitable block found, growing heap\n");
		size_t needed =
			(effective + sizeof(block_t) + PAGE_SIZE - 1) / PAGE_SIZE + 1;

		if (!grow_heap(heap_ctx, needed))
			return NULL;

		return kmalloc(size);
	}

	freelist_remove(chosen);

	const size_t min_remain = sizeof(block_t) + ALIGNMENT;

	if (chosen->size >= effective + min_remain) {
		block_t *rem =
			(block_t *)((uint8_t *)chosen + sizeof(block_t) + effective);

		rem->size =
			ALIGN_DOWN(chosen->size - effective - sizeof(block_t), ALIGNMENT);
		rem->user_size = 0;
		rem->alloc_size = 0;
		rem->prev = rem->next = NULL;
		set_check(rem);

		freelist_insert(rem);
		chosen->size = effective;
	}

	chosen->user_size = user_sz;
	chosen->alloc_size = effective;
	set_check(chosen);
	write_canaries(chosen);

	return (uint8_t *)chosen + sizeof(block_t) + CANARY_SIZE;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	block_t *b = (block_t *)((uint8_t *)ptr - CANARY_SIZE - sizeof(block_t));

	if (!validate(b))
		return;

	if (!check_canaries(b))
		return;

	b->user_size = 0;
	b->alloc_size = 0;

	freelist_insert(b);

	if (b->next &&
		(uint8_t *)b + sizeof(block_t) + b->size == (uint8_t *)b->next) {
		block_t *n = b->next;
		if (validate(n)) {
			b->size += sizeof(block_t) + n->size;
			freelist_remove(n);
			set_check(b);
		}
	}

	if (b->prev &&
		(uint8_t *)b->prev + sizeof(block_t) + b->prev->size == (uint8_t *)b) {
		block_t *p = b->prev;
		if (validate(p)) {
			p->size += sizeof(block_t) + b->size;
			freelist_remove(b);
			set_check(p);
		}
	}
}

void heap_switch_ctx(vctx_t *ctx)
{
	if (!ctx) {
		warn("NULL vctx passed!\n");
		return;
	}
	heap_ctx = ctx;
}