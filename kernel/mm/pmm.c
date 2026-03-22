/*********************************************************************************/
/* Module Name:  pmm.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#include <boot/axprot.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <lib/bitmap.h>
#include <lib/align.h>
#include <lib/string.h>
#include <sys/panic.h>
#include <sys/spinlock.h>
#include <test/pmm_test.h>
#include <test/test.h>
#include <aurix.h>

#define PAGE_CACHE_SIZE 1024
#define MIN_ALIGN PAGE_SIZE
#define BITMAP_WORD_SIZE (sizeof(uint64_t) * 8)

uint64_t bitmap_pages;
uint64_t bitmap_size;
uint8_t *bitmap;
uint64_t used_pages;
uint64_t usable_pages;
static uint64_t free_pages;
static spinlock_t pmm_lock;
static uint64_t page_cache[PAGE_CACHE_SIZE];
static size_t cache_size;
static size_t cache_index;
static uint32_t *page_refcounts;
static uint64_t refcount_entries;

static inline bool is_aligned(void *addr, size_t align)
{
	return ((uintptr_t)addr % align) == 0;
}

static char *type_to_str(int type)
{
	switch (type) {
	case AURIX_MMAP_RESERVED:
		return "reserved";
	case AURIX_MMAP_ACPI_RECLAIMABLE:
		return "acpi reclaimable";
	case AURIX_MMAP_ACPI_MAPPED_IO:
		return "acpi mapped io";
	case AURIX_MMAP_ACPI_MAPPED_IO_PORTSPACE:
		return "acpi mapped io portspace";
	case AURIX_MMAP_ACPI_NVS:
		return "acpi nvs";
	case AURIX_MMAP_KERNEL:
		return "kernel";
	case AURIX_MMAP_BOOTLOADER_RECLAIMABLE:
		return "bootloader reclaimable";
	case AURIX_MMAP_FRAMEBUFFER:
		return "framebuffer";
	case AURIX_MMAP_USABLE:
		return "usable";
	default:
		return "unknown";
	}
}

void pmm_init(void)
{
	spinlock_init(&pmm_lock);

	uint64_t high = 0;
	usable_pages = 0;
	free_pages = 0;

	debug("Dumping memory map:\n");
	for (uint64_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];

		if (e->type == AURIX_MMAP_USABLE) {
			uint64_t top = e->base + e->size;
			if (top > high)
				high = top;
			usable_pages += e->size / PAGE_SIZE;
		}

		debug("Entry %u: 0x%llx, size=%llu bytes, type=%s\n", i, e->base,
			  e->size, type_to_str(e->type));
	}

	bitmap_pages = DIV_ROUND_UP(high, PAGE_SIZE);
	bitmap_size = DIV_ROUND_UP(bitmap_pages, 8);
	used_pages = bitmap_pages;

	for (uint64_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];
		if (e->type == AURIX_MMAP_USABLE && e->base != 0 &&
			e->size >= bitmap_size) {
			bitmap = (uint8_t *)PHYS_TO_VIRT(e->base);
			memset(bitmap, 0xFF, bitmap_size);
			uint64_t bmp_pages = ALIGN_UP(bitmap_size, PAGE_SIZE) / PAGE_SIZE;
			e->base += bmp_pages * PAGE_SIZE;
			e->size -= bmp_pages * PAGE_SIZE;
			if (usable_pages >= bmp_pages)
				usable_pages -= bmp_pages;
			break;
		}
	}

	if (!bitmap) {
		error("Failed to allocate bitmap!\n");
		kpanicf(NULL, "pmm: failed to allocate bitmap");
	}

	for (uint64_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];
		if (e->type == AURIX_MMAP_USABLE) {
			pfree((void *)e->base, ALIGN_UP(e->size, PAGE_SIZE) / PAGE_SIZE);
		}
	}

	// NULL should be reserved
	if (!bitmap_get(bitmap, 0)) {
		bitmap_set(bitmap, 0);
		if (free_pages > 0)
			free_pages--;
		used_pages++;
	} else {
		bitmap_set(bitmap, 0);
	}

	uint64_t refcount_bytes = bitmap_pages * sizeof(uint32_t);
	uint64_t refcount_pages = ALIGN_UP(refcount_bytes, PAGE_SIZE) / PAGE_SIZE;
	if (refcount_pages > 0) {
		void *ref_phys = palloc(refcount_pages);
		if (!ref_phys) {
			error("pmm: failed to allocate refcount table\n");
			kpanicf(NULL, "pmm: failed to allocate refcount table");
		}
		page_refcounts = (uint32_t *)PHYS_TO_VIRT(ref_phys);
		memset(page_refcounts, 0, refcount_pages * PAGE_SIZE);
		refcount_entries = bitmap_pages;
	}

	// Register tests
#ifdef CONFIG_BUILD_TESTS
	TEST_ADD(pmm_test);
#endif
}

void pmm_reclaim_bootparms()
{
	if (boot_params->mmap_entries == 0 || boot_params->mmap_entries > 1000) {
		error("Invalid mmap_entries: %llu\n", boot_params->mmap_entries);
		return;
	}

	for (uint64_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];

		if (e->type == AURIX_MMAP_BOOTLOADER_RECLAIMABLE ||
			e->type == AURIX_MMAP_ACPI_RECLAIMABLE) {
			uint64_t pages = ALIGN_UP(e->size, PAGE_SIZE) / PAGE_SIZE;

			if (e->base >= VIRT_TO_PHYS(bitmap) &&
				e->base < VIRT_TO_PHYS(bitmap) + bitmap_size) {
				trace(
					"Skipping reclaim of bitmap region: base=0x%llx, size=%llu\n",
					e->base, e->size);
				continue;
			}

			e->type = AURIX_MMAP_USABLE;
			usable_pages += pages;
			pfree((void *)e->base, pages);
		}
	}
}

void *palloc(size_t pages)
{
	if (!bitmap || bitmap_pages == 0 || bitmap_size == 0) {
		error("pmm: bitmap not initialized (pages=%llu size=%llu)\n",
			  bitmap_pages, bitmap_size);
		kpanicf(NULL, "pmm: not initialized");
	}

	if (pages == 0) {
		warn("palloc: zero pages requested\n");
		return NULL;
	}

	if (pages > free_pages) {
		warn("palloc: request too large (pages=%zu free=%llu)\n", pages,
			 free_pages);
		return NULL;
	}

	spinlock_acquire(&pmm_lock);

	void *addr = NULL;
	uint64_t consecutive = 0;

	for (uint64_t bit = 0; bit < bitmap_pages; bit++) {
		if (!bitmap_get(bitmap, bit)) {
			if (++consecutive == pages) {
				uint64_t start = bit - pages + 1;
				for (uint64_t k = 0; k < pages; k++) {
					bitmap_set(bitmap, start + k);
					if (page_refcounts && (start + k) < refcount_entries)
						page_refcounts[start + k] = 1;
				}

				free_pages -= pages;
				used_pages += pages;

				addr = (void *)(start * PAGE_SIZE);
				memset((void *)PHYS_TO_VIRT(addr), 0, pages * PAGE_SIZE);
				goto end;
			}
		} else {
			consecutive = 0;
		}
	}

end:
	spinlock_release(&pmm_lock);
	return addr;
}

void pfree(void *ptr, size_t pages)
{
	if (!bitmap || bitmap_pages == 0 || bitmap_size == 0) {
		error("pmm: bitmap not initialized (pages=%llu size=%llu)\n",
			  bitmap_pages, bitmap_size);
		kpanicf(NULL, "pmm: not initialized");
	}

	if (!ptr) {
		warn("pfree: NULL ptr\n");
		return;
	}

	if (!is_aligned(ptr, MIN_ALIGN)) {
		warn("pfree: unaligned ptr=%p\n", ptr);
		return;
	}

	if (pages == 0) {
		warn("pfree: zero pages for ptr=%p\n", ptr);
		return;
	}

	spinlock_acquire(&pmm_lock);

	uint64_t start = (uint64_t)ptr / PAGE_SIZE;

	if (start + pages > bitmap_pages) {
		error("pfree: out of range (start=%llu pages=%zu bitmap_pages=%llu)\n",
			  start, pages, bitmap_pages);
		kpanicf(NULL, "pmm: pfree range out of bounds");
	}

	for (size_t i = 0; i < pages; i++) {
		if (bitmap_get(bitmap, start + i)) {
			bitmap_clear(bitmap, start + i);
			free_pages++;
			if (used_pages > 0)
				used_pages--;
			if (page_refcounts && (start + i) < refcount_entries)
				page_refcounts[start + i] = 0;

			if (pages == 1 && cache_index < cache_size) {
				page_cache[cache_index++] = start;
			}
		}
	}

	spinlock_release(&pmm_lock);
}

void pmm_ref_inc(uintptr_t phys, size_t pages)
{
	if (!page_refcounts || pages == 0)
		return;

	phys = ALIGN_DOWN(phys, PAGE_SIZE);
	for (size_t i = 0; i < pages; i++) {
		uint64_t idx = (phys / PAGE_SIZE) + i;
		spinlock_acquire(&pmm_lock);
		if (idx >= refcount_entries) {
			spinlock_release(&pmm_lock);
			warn("pmm_ref_inc: out of range idx=%llu\n", idx);
			continue;
		}
		page_refcounts[idx]++;
		spinlock_release(&pmm_lock);
	}
}

void pmm_ref_dec(uintptr_t phys, size_t pages)
{
	if (pages == 0)
		return;
	if (!page_refcounts) {
		pfree((void *)ALIGN_DOWN(phys, PAGE_SIZE), pages);
		return;
	}

	phys = ALIGN_DOWN(phys, PAGE_SIZE);
	for (size_t i = 0; i < pages; i++) {
		uint64_t idx = (phys / PAGE_SIZE) + i;
		bool free_page = false;
		spinlock_acquire(&pmm_lock);
		if (idx >= refcount_entries) {
			spinlock_release(&pmm_lock);
			warn("pmm_ref_dec: out of range idx=%llu\n", idx);
			continue;
		}
		if (page_refcounts[idx] == 0) {
			spinlock_release(&pmm_lock);
			warn("pmm_ref_dec: underflow idx=%llu\n", idx);
			continue;
		}
		page_refcounts[idx]--;
		if (page_refcounts[idx] == 0)
			free_page = true;
		spinlock_release(&pmm_lock);

		if (free_page)
			pfree((void *)(phys + i * PAGE_SIZE), 1);
	}
}

uint32_t pmm_refcount(uintptr_t phys)
{
	if (!page_refcounts)
		return 0;
	phys = ALIGN_DOWN(phys, PAGE_SIZE);
	uint64_t idx = phys / PAGE_SIZE;
	spinlock_acquire(&pmm_lock);
	if (idx >= refcount_entries) {
		spinlock_release(&pmm_lock);
		warn("pmm_refcount: out of range idx=%llu\n", idx);
		return 0;
	}
	uint32_t count = page_refcounts[idx];
	spinlock_release(&pmm_lock);
	return count;
}

uint64_t pmm_free_pages(void)
{
	return free_pages;
}

uint64_t pmm_used_pages(void)
{
	return used_pages;
}

uint64_t pmm_usable_pages(void)
{
	return usable_pages;
}
