/*********************************************************************************/
/* Module Name:  ff.c                                                             */
/* Project:      AurixOS                                                          */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy                                             */
/*                                                                               */
/* This source is subject to the MIT License.                                     */
/* See License.txt in the root of this repository.                                */
/* All other rights reserved.                                                     */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <mm/heap.h>
#include <mm/heap/ff.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <lib/align.h>
#include <debug/log.h>
#include <sys/panic.h>
#include <test/heap_test.h>
#include <test/test.h>
#include <config.h>
#include <aurix.h>
#include <string.h>
#include <sys/spinlock.h>

#define ALIGNMENT PAGE_SIZE
#define USER_ALIGNMENT 16
#define CANARY_SIZE sizeof(uint64_t)
#define CANARY_VALUE 0xdeadbeefdeadbeefULL
#define CHECK_MAGIC 0xfeedfacefeedfaceULL

static void *pool = NULL;
static size_t pool_pages = 0;
static block_t *freelist = NULL;
static vctx_t *heap_ctx = NULL;

static spinlock_t heap_lock;

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
		kpanicf(NULL, "HEAP CORRUPTION: block header %p invalid", (void *)b);
	}

	if ((uint8_t *)b + sizeof(block_t) + b->size >
		(uint8_t *)pool + pool_pages * PAGE_SIZE) {
		kpanicf(NULL, "HEAP BLOCK OUT OF BOUNDS: %p", (void *)b);
	}

	return 1;
}

static uint64_t *leading_canary_ptr(const block_t *b)
{
	return (uint64_t *)((uint8_t *)b + sizeof(block_t));
}

static uint64_t *trailing_canary_ptr(const block_t *b)
{
	return (uint64_t *)((uint8_t *)b + sizeof(block_t) + CANARY_SIZE +
						b->user_size);
}

static void write_canaries(block_t *b)
{
	*leading_canary_ptr(b) = CANARY_VALUE;
	*trailing_canary_ptr(b) = CANARY_VALUE;
}

static int check_canaries(const block_t *b)
{
	if (b->alloc_size < b->user_size + 2 * CANARY_SIZE) {
		kpanicf(NULL, "HEAP CORRUPTION: invalid size fields (%p)", (void *)b);
	}

	if (*leading_canary_ptr(b) != CANARY_VALUE) {
		kpanicf(NULL, "HEAP OVERFLOW: leading canary corrupted (%p)",
				(void *)b);
	}
	if (*trailing_canary_ptr(b) != CANARY_VALUE) {
		kpanicf(NULL, "HEAP OVERFLOW: trailing canary corrupted (%p)",
				(void *)b);
	}
	return 1;
}

/* NO LOCKS HERE */
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
	spinlock_init(&heap_lock);

	pool = valloc(ctx, FF_POOL_SIZE, VALLOC_RW);
	if (!pool) {
		kpanic(NULL, "Failed to allocate initial heap");
	}

	pool_pages = FF_POOL_SIZE;

	block_t *b = (block_t *)pool;
	b->size = ALIGN_DOWN(FF_POOL_SIZE * PAGE_SIZE - sizeof(block_t), ALIGNMENT);
	b->user_size = 0;
	b->alloc_size = 0;
	b->prev = b->next = NULL;
	set_check(b);

	freelist = b;

#if CONFIG_BUILD_TESTS
	TEST_ADD(heap_test);
#endif
}

void *kmalloc(size_t size)
{
	if (size == 0) {
		warn("kmalloc: zero size requested\n");
		return NULL;
	}

	spinlock_acquire(&heap_lock);

	size_t user_sz = ALIGN_UP(size, USER_ALIGNMENT);
	size_t payload = user_sz + 2 * CANARY_SIZE;
	size_t effective = ALIGN_UP(payload, ALIGNMENT);

	block_t *cur = freelist;
	block_t *chosen = NULL;

	while (cur) {
		if (!validate(cur)) {
			spinlock_release(&heap_lock);
			return NULL;
		}

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

		if (!grow_heap(heap_ctx, needed)) {
			spinlock_release(&heap_lock);
			return NULL;
		}

		cur = freelist;
		while (cur) {
			if (cur->size >= effective) {
				chosen = cur;
				break;
			}
			cur = cur->next;
		}
		if (!chosen) {
			spinlock_release(&heap_lock);
			return NULL;
		}
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

	spinlock_release(&heap_lock);

	return (uint8_t *)chosen + sizeof(block_t) + CANARY_SIZE;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	spinlock_acquire(&heap_lock);

	block_t *b = (block_t *)((uint8_t *)ptr - CANARY_SIZE - sizeof(block_t));

	if (!validate(b) || !check_canaries(b)) {
		spinlock_release(&heap_lock);
		return;
	}

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

	spinlock_release(&heap_lock);
}

void *krealloc(void *ptr, size_t size)
{
	if (!ptr)
		return kmalloc(size);

	if (size == 0) {
		kfree(ptr);
		return NULL;
	}

	spinlock_acquire(&heap_lock);

	block_t *b = (block_t *)((uint8_t *)ptr - CANARY_SIZE - sizeof(block_t));

	if (!validate(b) || !check_canaries(b)) {
		spinlock_release(&heap_lock);
		return NULL;
	}

	size_t new_user = ALIGN_UP(size, USER_ALIGNMENT);
	size_t new_payload = new_user + 2 * CANARY_SIZE;
	size_t new_effective = ALIGN_UP(new_payload, ALIGNMENT);

	if (b->size >= new_effective) {
		size_t min_remain = sizeof(block_t) + ALIGNMENT;

		if (b->size >= new_effective + min_remain) {
			block_t *rem =
				(block_t *)((uint8_t *)b + sizeof(block_t) + new_effective);

			rem->size = ALIGN_DOWN(b->size - new_effective - sizeof(block_t),
								   ALIGNMENT);
			rem->user_size = 0;
			rem->alloc_size = 0;
			rem->prev = rem->next = NULL;
			set_check(rem);

			freelist_insert(rem);
			b->size = new_effective;
		}

		b->user_size = new_user;
		b->alloc_size = new_effective;
		write_canaries(b);
		set_check(b);

		spinlock_release(&heap_lock);
		return ptr;
	}

	block_t *next = (block_t *)((uint8_t *)b + sizeof(block_t) + b->size);

	if ((uint8_t *)next < (uint8_t *)pool + pool_pages * PAGE_SIZE &&
		validate(next)) {
		block_t *cur = freelist;
		while (cur) {
			if (cur == next)
				break;
			cur = cur->next;
		}

		if (cur) {
			size_t combined = b->size + sizeof(block_t) + next->size;

			if (combined >= new_effective) {
				freelist_remove(next);

				b->size = combined;

				size_t min_remain = sizeof(block_t) + ALIGNMENT;

				if (b->size >= new_effective + min_remain) {
					block_t *rem = (block_t *)((uint8_t *)b + sizeof(block_t) +
											   new_effective);

					rem->size = ALIGN_DOWN(
						b->size - new_effective - sizeof(block_t), ALIGNMENT);
					rem->user_size = 0;
					rem->alloc_size = 0;
					rem->prev = rem->next = NULL;
					set_check(rem);

					freelist_insert(rem);
					b->size = new_effective;
				}

				b->user_size = new_user;
				b->alloc_size = new_effective;
				write_canaries(b);
				set_check(b);

				spinlock_release(&heap_lock);
				return ptr;
			}
		}
	}

	spinlock_release(&heap_lock);

	void *new_ptr = kmalloc(size);
	if (!new_ptr)
		return NULL;

	memcpy(new_ptr, ptr, b->user_size);
	kfree(ptr);

	return new_ptr;
}

void heap_switch_ctx(vctx_t *ctx)
{
	if (!ctx) {
		warn("NULL vctx passed!\n");
		return;
	}
	heap_ctx = ctx;
}
