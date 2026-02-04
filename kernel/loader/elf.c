/*********************************************************************************/
/* Module Name:  elf.c */
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

#include <lib/align.h>
#include <lib/string.h>
#include <loader/elf.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <aurix.h>

#include <stdint.h>

/* https://github.com/KevinAlavik/nekonix/blob/main/kernel/src/proc/elf.c */
/* Thanks, Kevin <3 */

uintptr_t elf32_load(char *data, uintptr_t *addr, size_t *size,
					 pagetable *pagemap)
{
	(void)data;
	(void)addr;
	(void)size;
	(void)pagemap;
	return 0;
}

uintptr_t elf64_load(char *data, uintptr_t *addr, size_t *size,
					 pagetable *pagemap)
{
	Elf64_Ehdr *header = (Elf64_Ehdr *)data;
	Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)data + header->e_phoff);

	uint64_t max_align = 0;
	uint64_t exec_size = 0;

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD)
			continue;

		if (ph[i].p_align > max_align) {
			max_align = ph[i].p_align;
		}

		exec_size += ph[i].p_memsz;
	}

	if (size != NULL)
		*size = exec_size;

	uint64_t phys =
		(uint64_t)kmalloc(ALIGN_UP(exec_size, PAGE_SIZE) + PAGE_SIZE) & ~0xFFF;
	if (!phys) {
		error("Failed to allocate memory for executable!\n");
		return 0;
	}

	*addr = phys;

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0)
			continue;

		uint64_t aligned_vaddr = ph[i].p_vaddr & ~(ph[i].p_align - 1);

		uint64_t flags = VMM_PRESENT;
		if (ph[i].p_flags & PF_W)
			flags |= VMM_WRITABLE;
		if (!(ph[i].p_flags & PF_X))
			flags |= VMM_NX;

		uint64_t virt = (uint64_t)(phys + (ph[i].p_vaddr - aligned_vaddr));

		debug("phys=0x%llx, virt=0x%llx, psize=%lu, msize=%lu\n", phys,
			  ph[i].p_vaddr, ph[i].p_filesz, ph[i].p_memsz);

		map_pages(NULL, aligned_vaddr, phys, (uint64_t)ph[i].p_memsz, flags);
		map_pages(pagemap, aligned_vaddr, phys, (uint64_t)ph[i].p_memsz, flags);
		memset((void *)virt, 0, ph[i].p_memsz);
		memcpy((void *)virt, data + ph[i].p_offset, ph[i].p_filesz);

		if (ph[i].p_filesz < ph[i].p_memsz) {
			memset((void *)(virt + ph[i].p_filesz), 0,
				   ph[i].p_memsz - ph[i].p_filesz);
		}

		phys += ph[i].p_memsz;
		phys = ALIGN_UP(phys, PAGE_SIZE);
	}

	info("ELF loaded successfully, entry point: 0x%llx\n", header->e_entry);
	return (uintptr_t)header->e_entry;
}

uintptr_t elf_load(char *data, uintptr_t *addr, size_t *size,
				   pagetable *pagemap)
{
	Elf32_Ehdr *header = (Elf32_Ehdr *)data;

	if (header->e_ident[EI_MAG0] != ELFMAG0 ||
		header->e_ident[EI_MAG1] != ELFMAG1 ||
		header->e_ident[EI_MAG2] != ELFMAG2 ||
		header->e_ident[EI_MAG3] != ELFMAG3) {
		error("elf_load(): Invalid ELF magic: 0x%.1x%.1x%.1x%.1x",
			  header->e_ident[EI_MAG0], header->e_ident[EI_MAG1],
			  header->e_ident[EI_MAG2], header->e_ident[EI_MAG3]);
		return 0;
	}

	if (header->e_ident[EI_CLASS] != ELFCLASS64) {
		error("elf_load(): Unsupported ELF class: %u",
			  header->e_ident[EI_CLASS]);
		return 0;
	}

	if (header->e_machine == 20 || header->e_machine == EM_386 ||
		header->e_machine == 40) {
		return elf32_load(data, addr, size, pagemap);
	} else if (header->e_machine == EM_AMD64) {
		return elf64_load(data, addr, size, pagemap);
	}

	error("elf_load(): Unsupported ELF machine: %u", header->e_machine);
	return 0;
}
