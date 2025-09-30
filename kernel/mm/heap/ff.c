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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/
#include <mm/heap.h>
#include <mm/heap/ff.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <lib/align.h>
#include <debug/print.h>

void *pool;
block_t *freelist = NULL;

void heap_init(vctx_t *ctx)
{
	pool = valloc(ctx, FF_POOL_SIZE, VALLOC_RW);
	if (!pool)
		klog("fatal: Failed to allocate memory for heap pool!\n");
	freelist = (block_t *)pool;
	freelist->size = (FF_POOL_SIZE * PAGE_SIZE) - sizeof(block_t);
	freelist->next = NULL;
}

void *kmalloc(size_t size)
{
	if (size == 0)
		return NULL;

	size = ALIGN_UP(size, 8);

	block_t *prev = NULL;
	block_t *cur = freelist;

	while (cur) {
		if (cur->size >= size) {
			size_t tot_size = sizeof(block_t) + size;

			if (cur->size >= ALIGN_UP(tot_size + sizeof(block_t), 8)) {
				block_t *new_block = (block_t *)((uint8_t *)cur + tot_size);
				new_block->size = cur->size - tot_size;
				new_block->next = cur->next;

				cur->size = size;

				if (prev)
					prev->next = new_block;
				else
					freelist = new_block;
			} else {
				if (prev)
					prev->next = cur->next;
				else
					freelist = cur->next;
			}

			return (void *)((uint8_t *)cur + sizeof(block_t));
		}
		prev = cur;
		cur = cur->next;
	}

	return NULL;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));

	block_t **cur = &freelist;
	while (*cur && *cur < block) {
		cur = &(*cur)->next;
	}

	block->next = *cur;
	*cur = block;

	if (block->next && (uint8_t *)block + sizeof(block_t) + block->size ==
						   (uint8_t *)block->next) {
		block->size += sizeof(block_t) + block->next->size;
		block->next = block->next->next;
	}

	if (cur != &freelist) {
		block_t *prev = freelist;
		while (prev->next != block)
			prev = prev->next;
		if ((uint8_t *)prev + sizeof(block_t) + prev->size ==
			(uint8_t *)block) {
			prev->size += sizeof(block_t) + block->size;
			prev->next = block->next;
		}
	}
}