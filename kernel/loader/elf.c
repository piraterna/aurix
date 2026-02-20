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
#include <mm/pmm.h>
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

	size_t pages = ALIGN_UP(exec_size, PAGE_SIZE) / PAGE_SIZE;
	uintptr_t phys = (uintptr_t)palloc(pages);
	if (!phys) {
		error("Failed to allocate memory for executable!\n");
		return 0;
	}

	*addr = (uintptr_t)phys;
	uintptr_t cur_phys = phys;

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0)
			continue;

		uint64_t aligned_vaddr = ph[i].p_vaddr & ~(ph[i].p_align - 1);

		uint64_t flags = VMM_PRESENT;
		if (ph[i].p_flags & PF_W)
			flags |= VMM_WRITABLE;
		if (!(ph[i].p_flags & PF_X))
			flags |= VMM_NX;

		uintptr_t virt = cur_phys + (ph[i].p_vaddr - aligned_vaddr);

		debug("phys=0x%llx, virt=0x%llx, psize=%lu, msize=%lu\n", phys,
			  ph[i].p_vaddr, ph[i].p_filesz, ph[i].p_memsz);

		map_pages(pagemap, aligned_vaddr, phys, (uint64_t)ph[i].p_memsz, flags);
		memset((void *)PHYS_TO_VIRT(virt), 0, ph[i].p_memsz);
		memcpy((void *)PHYS_TO_VIRT(virt), data + ph[i].p_offset, ph[i].p_filesz);

		if (ph[i].p_filesz < ph[i].p_memsz) {
			memset((void *)PHYS_TO_VIRT(virt + ph[i].p_filesz), 0,
				   ph[i].p_memsz - ph[i].p_filesz);
		}

		cur_phys += ph[i].p_memsz;
		cur_phys = ALIGN_UP(cur_phys, PAGE_SIZE);
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

static Elf64_Shdr *elf64_find_section(Elf64_Ehdr *ehdr, const char *name)
{
	Elf64_Shdr *shdr = (Elf64_Shdr *)((uint8_t *)ehdr + ehdr->e_shoff);
	Elf64_Shdr *shstrtab = &shdr[ehdr->e_shstrndx];
	const char *shstrtab_data = (const char *)ehdr + shstrtab->sh_offset;

	for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
		const char *sec_name = shstrtab_data + shdr[i].sh_name;
		if (strcmp(sec_name, name) == 0) {
			return &shdr[i];
		}
	}
	return NULL;
}

static bool elf64_get_symtab(Elf64_Ehdr *ehdr, Elf64_Sym **symtab_out,
							 size_t *nsyms_out, const char **strtab_out)
{
	Elf64_Shdr *symtab_sh = elf64_find_section(ehdr, ".symtab");
	if (!symtab_sh || symtab_sh->sh_type != SHT_SYMTAB) {
		return false;
	}

	Elf64_Shdr *strtab_sh = elf64_find_section(ehdr, ".strtab");
	if (!strtab_sh || strtab_sh->sh_type != SHT_STRTAB) {
		return false;
	}

	*symtab_out = (Elf64_Sym *)((uint8_t *)ehdr + symtab_sh->sh_offset);
	*nsyms_out = symtab_sh->sh_size / sizeof(Elf64_Sym);
	*strtab_out = (const char *)ehdr + strtab_sh->sh_offset;

	return true;
}

uintptr_t elf_lookup_symbol(char *elf_data, const char *symbol_name)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;

	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr->e_ident[EI_CLASS] != ELFCLASS64 || ehdr->e_machine != EM_AMD64) {
		error("Invalid ELF header\n");
		return 0;
	}

	Elf64_Sym *symtab = NULL;
	size_t nsyms = 0;
	const char *strtab = NULL;

	if (!elf64_get_symtab(ehdr, &symtab, &nsyms, &strtab)) {
		debug("No .symtab or .strtab found\n");
		return 0;
	}

	for (size_t i = 0; i < nsyms; i++) {
		Elf64_Sym *sym = &symtab[i];

		if (sym->st_name == 0)
			continue;

		const char *name = strtab + sym->st_name;
		if (strcmp(name, symbol_name) != 0)
			continue;

		if (ELF64_ST_BIND(sym->st_info) != STB_GLOBAL ||
			sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_ABS) {
			continue;
		}

		debug("Found \"%s\" at %p\n", symbol_name, (uintptr_t)sym->st_value);
		return (uintptr_t)sym->st_value;
	}

	debug("Symbol '%s' not found\n", symbol_name);
	return 0;
}
