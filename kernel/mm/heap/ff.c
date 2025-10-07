/*********************************************************************************/
/* Module Name:  ff.c */
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
/* SOFTWARE.                                                                     */
/*********************************************************************************/
#include <mm/heap.h>
#include <mm/heap/ff.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <lib/align.h>
#include <debug/log.h>

#define ALIGNMENT 16
#define CANARY_SIZE sizeof(uint64_t)
#define CANARY_VALUE 0xdeadbeefdeadbeefULL
#define CHECK_MAGIC 0xfeedfacefeedfaceULL

static void *pool;
static block_t *freelist = NULL;

static size_t compute_check(block_t *b)
{
	return (uintptr_t)b->prev ^ (uintptr_t)b->next ^ b->size ^ b->user_size ^
		   CHECK_MAGIC;
}

static void set_check(block_t *b)
{
	b->check = compute_check(b);
}

static int validate(block_t *b)
{
	if (b->check != compute_check(b)) {
		error("Heap corruption detected in block %p!\n", (void *)b);
		return 0;
	}
	return 1;
}

void heap_init(vctx_t *ctx)
{
	pool = valloc(ctx, FF_POOL_SIZE, VALLOC_RW);
	if (!pool) /* TODO: call something like kpanic */ {
		error("Failed to allocate memory for heap pool!\n");
		for (;;)
			;
	}
	freelist = (block_t *)pool;
	freelist->prev = NULL;
	freelist->next = NULL;
	freelist->user_size = 0;
	freelist->size =
		ALIGN_DOWN((FF_POOL_SIZE * PAGE_SIZE) - sizeof(block_t), ALIGNMENT);
	set_check(freelist);
}

void *kmalloc(size_t size)
{
	if (size == 0)
		return NULL;

	size_t user_size = ALIGN_UP(size, ALIGNMENT);
	size_t effective_size = ALIGN_UP(user_size + CANARY_SIZE, ALIGNMENT);

	block_t *cur = freelist;
	while (cur) {
		if (!validate(cur))
			return NULL;
		if (cur->size >= effective_size) {
			size_t min_split_remainder = sizeof(block_t) + ALIGNMENT;
			if (cur->size >= effective_size + min_split_remainder) {
				block_t *new_block =
					(block_t *)((uint8_t *)cur + sizeof(block_t) +
								effective_size);
				new_block->size = cur->size - effective_size;
				new_block->user_size = 0;
				new_block->prev = cur->prev;
				new_block->next = cur->next;
				set_check(new_block);
				if (new_block->next) {
					new_block->next->prev = new_block;
					set_check(new_block->next);
				}
				if (new_block->prev) {
					new_block->prev->next = new_block;
					set_check(new_block->prev);
				}
				if (freelist == cur)
					freelist = new_block;
				cur->size = effective_size;
			} else {
				effective_size = cur->size;
				if (cur->prev) {
					cur->prev->next = cur->next;
					set_check(cur->prev);
				} else {
					freelist = cur->next;
				}
				if (cur->next) {
					cur->next->prev = cur->prev;
					set_check(cur->next);
				}
			}
			cur->prev = NULL;
			cur->next = NULL;
			cur->user_size = user_size;
			set_check(cur);
			void *ptr = (uint8_t *)cur + sizeof(block_t);
			*(uint64_t *)((uint8_t *)ptr + user_size) = CANARY_VALUE;
			return ptr;
		}
		cur = cur->next;
	}
	return NULL;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));
	if (!validate(block))
		return;

	uint64_t *canary_ptr = (uint64_t *)((uint8_t *)ptr + block->user_size);
	if (*canary_ptr != CANARY_VALUE) {
		error("Heap canary corruption at %p!\n", ptr);
		return;
	}

	block->user_size = 0;

	block_t *cur = freelist;
	block_t *ins_prev = NULL;
	while (cur && cur < block) {
		if (!validate(cur))
			return;
		ins_prev = cur;
		cur = cur->next;
	}

	block->next = cur;
	block->prev = ins_prev;
	set_check(block);

	if (ins_prev) {
		ins_prev->next = block;
		set_check(ins_prev);
	} else {
		freelist = block;
	}

	if (cur) {
		cur->prev = block;
		set_check(cur);
	}

	if (block->next && ((uint8_t *)block + sizeof(block_t) + block->size) ==
						   (uint8_t *)block->next) {
		if (!validate(block->next))
			return;
		block->size += sizeof(block_t) + block->next->size;
		block_t *next_next = block->next->next;
		block->next = next_next;
		set_check(block);
		if (next_next) {
			next_next->prev = block;
			set_check(next_next);
		}
	}

	if (block->prev && ((uint8_t *)block->prev + sizeof(block_t) +
						block->prev->size) == (uint8_t *)block) {
		if (!validate(block->prev))
			return;
		block->prev->size += sizeof(block_t) + block->size;
		block->prev->next = block->next;
		set_check(block->prev);
		if (block->next) {
			block->next->prev = block->prev;
			set_check(block->next);
		}
	}
}