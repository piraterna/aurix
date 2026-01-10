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

#define ALIGNMENT 16
#define CANARY_SIZE sizeof(uint64_t)
#define CANARY_VALUE 0xdeadbeefdeadbeefULL
#define CHECK_MAGIC 0xfeedfacefeedfaceULL

static void *pool = NULL;
static size_t pool_pages = 0;
static block_t *freelist = NULL;

// current heap ctx
static vctx_t *heap_ctx = NULL;

static size_t compute_check(const block_t *b)
{
	return (uintptr_t)b->prev ^ (uintptr_t)b->next ^ b->size ^ b->user_size ^
		   CHECK_MAGIC;
}

static void set_check(block_t *b)
{
	b->check = compute_check(b);
}

static int validate(const block_t *b)
{
	if (b->check != compute_check(b)) {
		critical("HEAP CORRUPTION: block header %p invalid\n", (void *)b);
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

static int grow_heap(vctx_t *ctx, size_t needed_pages)
{
	trace("Heap grow request: %zu pages\n", needed_pages);

	size_t add_pages = ALIGN_UP(needed_pages, 64);
	void *new_mem = valloc(ctx, add_pages, VALLOC_RW);
	if (!new_mem) {
		error("Heap growth failed: cannot allocate %zu pages\n", add_pages);
		return 0;
	}

	warn("Heap growing: +%zu pages (old=%zu new=%zu)\n", add_pages, pool_pages,
		 pool_pages + add_pages);

	block_t *new_block = (block_t *)new_mem;
	new_block->prev = NULL;
	new_block->next = NULL;
	new_block->user_size = 0;
	new_block->size = add_pages * PAGE_SIZE - sizeof(block_t);
	new_block->size = ALIGN_DOWN(new_block->size, ALIGNMENT);
	set_check(new_block);

	if (pool) {
		block_t *last = (block_t *)((uint8_t *)pool + pool_pages * PAGE_SIZE -
									sizeof(block_t));

		if (!validate(last))
			return 0;

		if ((uint8_t *)last + sizeof(block_t) + last->size ==
			(uint8_t *)new_block) {
			debug("Heap extension contiguous, merging blocks\n");
			last->size += sizeof(block_t) + new_block->size;
			set_check(last);
		} else {
			debug("Heap extension non-contiguous, new free block\n");
			new_block->next = freelist;
			if (freelist)
				freelist->prev = new_block;
			freelist = new_block;
		}
	} else {
		info("Heap pool created\n");
		pool = new_mem;
		freelist = new_block;
	}

	pool_pages += add_pages;
	return 1;
}

void heap_init(vctx_t *ctx)
{
	heap_ctx = ctx;
	const size_t initial_pages = FF_POOL_SIZE;
	pool = valloc(ctx, initial_pages, VALLOC_RW);
	if (!pool) {
		critical("Failed to allocate initial heap pool (%zu pages)\n",
				 initial_pages);
		for (;;)
			;
	}

	pool_pages = initial_pages;

	freelist = (block_t *)pool;
	freelist->prev = NULL;
	freelist->next = NULL;
	freelist->user_size = 0;
	freelist->size =
		ALIGN_DOWN(initial_pages * PAGE_SIZE - sizeof(block_t), ALIGNMENT);
	set_check(freelist);
}

void *kmalloc(size_t size)
{
	if (size == 0) {
		warn("kmalloc(0) called\n");
		return NULL;
	}

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
			(effective + sizeof(block_t) + ALIGNMENT - 1) / PAGE_SIZE + 1;

		if (!grow_heap(heap_ctx, needed))
			return NULL;

		return kmalloc(size);
	}

	const size_t min_remain = sizeof(block_t) + ALIGNMENT;

	if (chosen->size >= effective + min_remain) {
		block_t *remainder =
			(block_t *)((uint8_t *)chosen + sizeof(block_t) + effective);

		remainder->size =
			ALIGN_DOWN(chosen->size - effective - sizeof(block_t), ALIGNMENT);
		remainder->user_size = 0;
		remainder->prev = chosen->prev;
		remainder->next = chosen->next;
		set_check(remainder);

		if (remainder->prev) {
			remainder->prev->next = remainder;
			set_check(remainder->prev);
		}
		if (remainder->next) {
			remainder->next->prev = remainder;
			set_check(remainder->next);
		}
		if (freelist == chosen)
			freelist = remainder;

		chosen->size = effective;
	} else {
		if (chosen->prev) {
			chosen->prev->next = chosen->next;
			set_check(chosen->prev);
		} else {
			freelist = chosen->next;
		}
		if (chosen->next) {
			chosen->next->prev = chosen->prev;
			set_check(chosen->next);
		}
	}

	chosen->prev = NULL;
	chosen->next = NULL;
	chosen->user_size = user_sz;
	chosen->alloc_size = effective;
	chosen->size = effective;
	set_check(chosen);
	write_canaries(chosen);

	void *ret = (uint8_t *)chosen + sizeof(block_t) + CANARY_SIZE;
	return ret;
}

void kfree(void *ptr)
{
	if (!ptr) {
		warn("kfree(NULL) ignored\n");
		return;
	}

	block_t *b = (block_t *)((uint8_t *)ptr - CANARY_SIZE - sizeof(block_t));

	if (!validate(b))
		return;

	if (!check_canaries(b))
		return;

	b->user_size = 0;

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

	if (b->next &&
		(uint8_t *)b + sizeof(block_t) + b->size == (uint8_t *)b->next) {
		if (!validate(b->next))
			return;

		b->size += sizeof(block_t) + b->next->size;
		b->next = b->next->next;
		set_check(b);

		if (b->next) {
			b->next->prev = b;
			set_check(b->next);
		}
	}

	if (b->prev &&
		(uint8_t *)b->prev + sizeof(block_t) + b->prev->size == (uint8_t *)b) {
		if (!validate(b->prev))
			return;

		b->prev->size += sizeof(block_t) + b->size;
		b->prev->next = b->next;
		set_check(b->prev);

		if (b->next) {
			b->next->prev = b->prev;
			set_check(b->next);
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
