/*********************************************************************************/
/* Module Name:  aurix.c                                                         */
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

#include <proto/aurix.h>
#include <loader/elf.h>
#include <mm/mman.h>
#include <mm/memmap.h>
#include <mm/vmm.h>
#include <vfs/vfs.h>
#include <print.h>
#include <axboot.h>
#include <efi.h>
#include <efilib.h>

extern __attribute__((noreturn)) void aurix_handoff(void *pagemap, void *stack, uint64_t entry, void *params);
extern char _aurix_handoff_start[], _aurix_handoff_end[];

void aurix_load(char *kernel_path)
{
	// read kernel -> test read
	char *kbuf = NULL;
	vfs_read(kernel_path, &kbuf);
	
	// TODO: Do something with the kernel :p
	pagetable *pm = create_pagemap();
	if (!pm) {
		debug("aurix_load(): Failed to create kernel pagemap! Halting...\n");
		// TODO: Halt
		while (1);
	}
	
	axboot_memmap *memmap = get_memmap(pm);
	(void)memmap;

	map_pages(pm, (uintptr_t)pm, (uintptr_t)pm, PAGE_SIZE, VMM_WRITABLE);
	map_pages(pm, (uintptr_t)_aurix_handoff_start, (uintptr_t)_aurix_handoff_start, (uint64_t)_aurix_handoff_end - (uint64_t)_aurix_handoff_start, 0);

	void *stack = mem_alloc(16*1024); // 16 KiB stack should be well more than enough
	if (!stack) {
		debug("aurix_load(): Failed to allocate stack! Halting...\n");
		while (1);
	}

	map_pages(pm, (uintptr_t)stack, (uintptr_t)stack, 16*1024, VMM_WRITABLE | VMM_NX);

	void *kernel_entry = (void *)elf_load(kbuf, pm);
	if (!kernel_entry) {
		debug("aurix_load(): Failed to load '%s'! Halting...\n", kernel_path);
		while (1);
	}
	// mem_free(kbuf);

	void *parameters = NULL;
	(void)parameters;

	debug("aurix_load(): Handoff state: pm=0x%llx, stack=0x%llx, kernel_entry=0x%llx\n", pm, stack, kernel_entry);

	// this triggers a #GP ????
	// aurix_handoff(pm, stack, (uint64_t)kernel_entry, (void *)parameters);
	// __builtin_unreachable();

	__asm__ volatile("movq %[pml4], %%cr3\n"
					"movq %[stack], %%rsp\n"
					"callq *%[entry]\n"
					:: [pml4]"r"(pm), [stack]"r"(stack + (16 * 1024)), [entry]"r"(kernel_entry) : "memory");
	__builtin_unreachable();
}
