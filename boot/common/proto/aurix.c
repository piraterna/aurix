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
#include <mm/vmm.h>
#include <vfs/vfs.h>
#include <print.h>

extern __attribute__((noreturn)) void aurix_handoff(void *pagemap, void *stack, uint64_t entry, void *params);
extern char _aurix_handoff_start[], _aurix_handoff_end[];

void aurix_load(char *kernel)
{
	// read kernel -> test read
	char *kbuf = NULL;
	vfs_read(kernel, &kbuf);

	// TODO: Do something with the kernel :p
	pagetable *pm = create_pagemap();
	if (!pm) {
		debug("aurix_load(): Failed to create kernel pagemap! Halting...\n");
		// TODO: Halt
		while (1);
	}

	map_pages(pm, (uintptr_t)_aurix_handoff_start, (uintptr_t)_aurix_handoff_start, (uint64_t)_aurix_handoff_end - (uint64_t)_aurix_handoff_start, VMM_PRESENT | VMM_USER | VMM_WRITABLE);

	void *stack = mem_alloc(16*1024); // 16 KiB stack should be well more than enough
	if (!stack) {
		debug("aurix_load(): Failed to allocate stack! Halting...\n");
		while (1);
	}

	void *kernel_entry = (void *)elf_load(kbuf, pm);
	if (!kernel_entry) {
		debug("aurix_load(): Failed to load '%s'! Halting...\n", kernel);
		mem_free(kbuf);
		while (1);
	}

	void *parameters = NULL;

	debug("aurix_load(): Handoff state: pm=0x%llx, stack=0x%llx, kernel_entry=0x%llx\n", pm, stack, kernel_entry);

	aurix_handoff(pm, (void *)((uint8_t)stack - 16*1024), (uint64_t)kernel_entry, (void *)parameters);

	// __asm__ volatile("movq %[pml4], %%cr3\n" :: [pml4]"r"(pm) : "memory");
	// __asm__ volatile("callq *%[entry]\n"
					// :: [entry]"r"(kernel_entry));

}