/*********************************************************************************/
/* Module Name:  pmm.c */
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

#include <mm/pmm.h>
#include <boot/aurix.h>
#include <lib/bitmap.h>
#include <lib/align.h>
#include <sys/spinlock.h>
#include <lib/string.h>
#include <debug/print.h>

#define PAGE_CACHE_SIZE 1024
#define MIN_ALIGN PAGE_SIZE
#define BITMAP_WORD_SIZE (sizeof(uint64_t) * 8)

uint64_t bitmap_pages;
uint64_t bitmap_size;
uint8_t *bitmap;
static uint64_t free_pages;
static spinlock_t pmm_lock;
static uint64_t page_cache[PAGE_CACHE_SIZE];
static size_t cache_size;
static size_t cache_index;

static uint64_t hhdm_offset = 0; // FIXME: actually put the legit hhdm offset
#define HIGHER_HALF(x) (x + hhdm_offset)

static inline bool is_aligned(void *addr, size_t align)
{
	return ((uintptr_t)addr % align) == 0;
}

void pmm_init(void)
{
	spinlock_init(&pmm_lock);

	uint64_t high = 0;
	free_pages = 0;

	for (uint64_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];
		if (e->type == AURIX_MMAP_USABLE) {
			uint64_t top = e->base + e->size;
			if (top > high)
				high = top;
			free_pages += e->size / PAGE_SIZE;
			klog("Usable memory region: 0x%.16llx -> 0x%.16llx\n", e->base,
				 e->base + e->size);
		}
	}

	bitmap_pages = high / PAGE_SIZE;
	bitmap_size = ALIGN_UP(bitmap_pages / 8, PAGE_SIZE);

	for (uint64_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];
		if (e->type == AURIX_MMAP_USABLE && e->size >= bitmap_size) {
			bitmap = (uint8_t *)(HIGHER_HALF(e->base));
			memset(bitmap, 0xFF, bitmap_size);
			e->base += bitmap_size;
			e->size -= bitmap_size;
			free_pages -= bitmap_size / PAGE_SIZE;
			break;
		}
	}

	cache_size = PAGE_CACHE_SIZE;
	cache_index = 0;
	memset(page_cache, 0, sizeof(page_cache));

	for (uint64_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];
		if (e->type == AURIX_MMAP_USABLE) {
			for (uint64_t j = e->base; j < e->base + e->size; j += PAGE_SIZE) {
				if ((j / PAGE_SIZE) < bitmap_pages) {
					bitmap_clear(bitmap, j / PAGE_SIZE);
				}
			}
		}
	}
}

void *palloc(size_t pages, bool higher_half)
{
	if (pages == 0 || pages > free_pages)
		return NULL;

	spinlock_acquire(&pmm_lock);

	if (pages == 1 && cache_index > 0) {
		void *addr = (void *)(page_cache[--cache_index] * PAGE_SIZE);
		bitmap_set(bitmap, (uint64_t)addr / PAGE_SIZE);
		free_pages--;
		memset(HIGHER_HALF(addr), 0, pages * PAGE_SIZE);
		spinlock_release(&pmm_lock);
		return higher_half ? (void *)((uint64_t)addr + hhdm_offset) : addr;
	}

	uint64_t word_count =
		(bitmap_pages + BITMAP_WORD_SIZE - 1) / BITMAP_WORD_SIZE;
	uint64_t *bitmap_words = (uint64_t *)bitmap;

	for (uint64_t i = 0; i < word_count; i++) {
		if (bitmap_words[i] != UINT64_MAX) {
			uint64_t start_bit = i * BITMAP_WORD_SIZE;
			uint64_t consecutive = 0;

			for (uint64_t j = 0;
				 j < BITMAP_WORD_SIZE && start_bit + j < bitmap_pages; j++) {
				if (!bitmap_get(bitmap, start_bit + j)) {
					if (++consecutive == pages) {
						for (uint64_t k = 0; k < pages; k++) {
							bitmap_set(bitmap, start_bit + j - pages + 1 + k);
						}
						free_pages -= pages;

						void *addr =
							(void *)((start_bit + j - pages + 1) * PAGE_SIZE);
						memset(HIGHER_HALF(addr), 0, pages * PAGE_SIZE);
						spinlock_release(&pmm_lock);
						return higher_half ?
								   (void *)((uint64_t)addr + hhdm_offset) :
								   addr;
					}
				} else {
					consecutive = 0;
				}
			}
		}
	}

	spinlock_release(&pmm_lock);
	return NULL;
}

void pfree(void *ptr, size_t pages)
{
	if (!ptr || !is_aligned(ptr, MIN_ALIGN))
		return;

	spinlock_acquire(&pmm_lock);

	uint64_t start =
		((uint64_t)ptr - (hhdm_offset * ((uint64_t)ptr >= hhdm_offset))) /
		PAGE_SIZE;

	if (start + pages > bitmap_pages) {
		spinlock_release(&pmm_lock);
		return;
	}

	for (size_t i = 0; i < pages; i++) {
		if (bitmap_get(bitmap, start + i)) {
			bitmap_clear(bitmap, start + i);
			free_pages++;

			if (pages == 1 && cache_index < cache_size) {
				page_cache[cache_index++] = start;
			}
		}
	}

	spinlock_release(&pmm_lock);
}