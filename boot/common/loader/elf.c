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
#include <mm/mman.h>
#include <mm/vmm.h>
#include <print.h>
#include <stdint.h>

/* https://github.com/KevinAlavik/nekonix/blob/main/kernel/src/proc/elf.c */
/* Thanks, Kevin <3 */

uintptr_t elf32_load(char *data, uintptr_t *addr, pagetable *pagemap)
{
	(void)data;
	(void)addr;
	(void)pagemap;
	return 0;
}

uintptr_t elf64_load(char *data, uintptr_t *addr, pagetable *pagemap)
{
	struct elf_header *header = (struct elf_header *)data;
	struct elf_program_header *ph =
		(struct elf_program_header *)((uint8_t *)data + header->e_phoff);

	uint64_t max_align = 0;

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD)
			continue;

		if (ph[i].p_align > max_align) {
			max_align = ph[i].p_align;
		}
	}

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		debug("elf64_load(): Segment %u:\n", i);
		debug("elf64_load(): - Type: %u\n", ph[i].p_type);
		debug("elf64_load(): - Flags: 0x%lx\n", ph[i].p_flags);
		debug("elf64_load(): - Offset: %llu\n", ph[i].p_offset);
		debug("elf64_load(): - Physical address: 0x%llx\n", ph[i].p_paddr);
		debug("elf64_load(): - Virtual address: 0x%llx\n", ph[i].p_vaddr);
		debug("elf64_load(): - File Size: %llu bytes\n", ph[i].p_filesz);
		debug("elf64_load(): - Memory Size: %llu bytes\n", ph[i].p_memsz);
		debug("elf64_load(): - Alignment: 0x%llx\n", ph[i].p_align);
	
		if (ph[i].p_type != PT_LOAD)
			continue;

		uint64_t aligned_vaddr = ph[i].p_vaddr & ~(ph[i].p_align - 1);

		uint64_t flags = VMM_PRESENT;
		if (ph[i].p_flags & PF_W)
			flags |= VMM_WRITABLE;
		if (!(ph[i].p_flags & PF_X))
			flags |= VMM_NX;

		uint64_t phys = (uint64_t)mem_alloc(ph[i].p_memsz + ph[i].p_vaddr -
											 aligned_vaddr + 4096) + 4096;
		phys &= ~0xFFF;
		uint64_t virt = (uint64_t)(phys + ph[i].p_vaddr - aligned_vaddr);

		if (!phys) {
			error("elf64_load(): Out of memory\n");
			return 0;
		}

		if (addr != NULL && *addr == 0) {
			*addr = phys;
		}

		debug("elf64_load(): phys=0x%llx, virt=0x%llx, psize=%lu, msize=%lu\n",
			  phys, ph[i].p_vaddr, ph[i].p_filesz, ph[i].p_memsz);

		map_pages(pagemap, aligned_vaddr, phys, ph[i].p_memsz, flags);
		memset((void *)virt, 0, ph[i].p_memsz);
		memcpy((void *)virt,
			   data + ph[i].p_offset, ph[i].p_filesz);

		if (ph[i].p_filesz < ph[i].p_memsz) {
			memset(
				(void *)(virt + ph[i].p_filesz),
				0, ph[i].p_memsz - ph[i].p_filesz);
		}
	}

	debug("elf64_load(): ELF loaded successfully, entry: 0x%llx\n",
		  header->e_entry);
	return (uintptr_t)header->e_entry;
}

uintptr_t elf_load(char *data, uintptr_t *addr, pagetable *pagemap)
{
	struct elf_header *header = (struct elf_header *)data;

	if (header->e_magic != ELF_MAGIC) {
		error("elf_load(): Invalid ELF magic: 0x%x", header->e_magic);
		return 0;
	}

	if (header->e_class != 2) {
		error("elf_load(): Unsupported ELF class: %u", header->e_class);
		return 0;
	}

	if (header->e_machine == 20 || header->e_machine == 3 ||
		header->e_machine == 40) {
		return elf32_load(data, addr, pagemap);
	} else if (header->e_machine == 62) {
		return elf64_load(data, addr, pagemap);
	}

	error("elf_load(): Unsupported ELF machine: %u", header->e_machine);
	return 0;
}
