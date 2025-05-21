/*********************************************************************************/
/* Module Name:  mman.c                                                          */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <arch/mm/paging.h>
#include <mm/mman.h>
#include <lib/string.h>
#include <print.h>
#include <stddef.h>

// NOTE: If any allocations fail, try increasing this number.
#define MAX_ALLOCATIONS 64

struct alloc_header allocation_list[MAX_ALLOCATIONS] = {0};

int find_alloc(void *addr)
{
	for (int i = 0; i < MAX_ALLOCATIONS; i++) {
		if (allocation_list[i].addr == addr) {
			return i;
		}
	}

	return -1;
}

int add_alloc(void *addr, size_t size)
{
	for (int i = 0; i < MAX_ALLOCATIONS; i++) {
		if (allocation_list[i].addr == 0) {
			allocation_list[i].addr = addr;
			allocation_list[i].size = size;
			return 1;
		}
	}

	return 0;
}

void remove_alloc(void *addr)
{
	int i = find_alloc(addr);
	if (i == -1) {
		return;
	}

	allocation_list[i].addr = 0;
	allocation_list[i].size = 0;
}

#ifndef AXBOOT_UEFI

#warning "Memory management is not implemented yet, expect runtime errors."

void *mem_alloc(size_t n)
{
	(void)n;

	return NULL;
}

int mem_allocat(void *addr, size_t npages)
{
	(void)addr;
	(void)npages;

	return 0;
}

void *mem_realloc(void *addr, size_t n)
{
	(void)addr;
	(void)n;

	return NULL;
}

void mem_free(void *addr)
{
	(void)addr;
}

#else

#include <efi.h>
#include <efilib.h>

void *mem_alloc(size_t n)
{
	EFI_STATUS status;
	void *alloc = NULL;

	status = gBootServices->AllocatePool(EfiLoaderData, (EFI_UINTN)n, &alloc);
	if (EFI_ERROR(status)) {
		debug("mem_alloc(): Couldn't allocate %u bytes: %s (%lx)\n", n, efi_status_to_str(status), status);
		return NULL;
	}

	add_alloc(alloc, n);

	debug("mem_alloc(): Allocated %u bytes\n", n);
	return alloc;
}

int mem_allocat(void *addr, size_t npages)
{
	EFI_STATUS status;

	status = gBootServices->AllocatePages(AllocateAddress, EfiLoaderData, (EFI_UINTN)npages, addr);
	if (EFI_ERROR(status)) {
		debug("mem_allocat(): Couldn't allocate %u pages at 0x%lx: %s (%lx)\n", npages, addr, efi_status_to_str(status), status);
		return 0;
	}

	add_alloc(addr, npages * PAGE_SIZE);

	return 1;
}

void *mem_realloc(void *addr, size_t n)
{
	size_t old_size;
	void *new = NULL;
	
	int i = find_alloc(addr);
	if (i == -1 || addr == NULL) {
		debug("mem_realloc(): Couldn't find allocation for 0x%lx, allocating new memory.\n");
		return mem_alloc(n);
	}

	old_size = allocation_list[i].size;

	new = mem_alloc(n);
	if (!new) {
		return NULL;
	}

	memcpy(new, addr, old_size);
	mem_free(addr);

	return new;
}

void mem_free(void *addr)
{
	EFI_STATUS status;

	if (addr == NULL) {
		return;
	}

	status = gBootServices->FreePool(addr);
	if (EFI_ERROR(status)) {
		debug("mem_free(): Couldn't free 0x%lx: %s (%lx)\n", addr, efi_status_to_str(status), status);
		return;
	}

	debug("mem_free(): Freed 0x%llx\n", addr);

	remove_alloc(addr);
}

#endif
