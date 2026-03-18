/*********************************************************************************/
/* Module Name:  elf.c */
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

#include <lib/align.h>
#include <lib/string.h>
#include <loader/elf.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sys/axapi.h>
#include <aurix.h>

#include <stdint.h>

/* https://github.com/KevinAlavik/nekonix/blob/main/kernel/src/proc/elf.c */
/* Thanks, Kevin <3 */

static void elf64_apply_relocations_ex(Elf64_Ehdr *header, uintptr_t phys_base,
									   uintptr_t load_base,
									   uintptr_t link_base);

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

	uintptr_t base_vaddr = (uintptr_t)-1;
	uintptr_t end_vaddr = 0;

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type == PT_INTERP) {
			error("elf64_load(): dynamic interpreter not supported (%s)\n",
				  (char *)((uintptr_t)data + ph[i].p_offset));
			return 0;
		}

		if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0)
			continue;

		uintptr_t seg_start = ALIGN_DOWN((uintptr_t)ph[i].p_vaddr, PAGE_SIZE);
		uintptr_t seg_end = ALIGN_UP(
			(uintptr_t)ph[i].p_vaddr + (uintptr_t)ph[i].p_memsz, PAGE_SIZE);

		if (seg_start < base_vaddr)
			base_vaddr = seg_start;
		if (seg_end > end_vaddr)
			end_vaddr = seg_end;
	}

	if (base_vaddr == (uintptr_t)-1 || end_vaddr <= base_vaddr) {
		error("elf64_load(): No loadable segments\n");
		return 0;
	}

	uintptr_t exec_size = end_vaddr - base_vaddr;

	if (size != NULL)
		*size = exec_size;

	size_t pages = (exec_size + PAGE_SIZE - 1) / PAGE_SIZE;
	uintptr_t phys_base = (uintptr_t)palloc(pages);
	if (!phys_base) {
		error("Failed to allocate memory for executable!\n");
		return 0;
	}

	*addr = (uintptr_t)phys_base;
	memset((void *)PHYS_TO_VIRT(phys_base), 0, exec_size);

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0)
			continue;

		uintptr_t aligned_vaddr =
			ALIGN_DOWN((uintptr_t)ph[i].p_vaddr, PAGE_SIZE);
		uintptr_t seg_off = (uintptr_t)ph[i].p_vaddr - aligned_vaddr;
		uintptr_t seg_size = seg_off + (uintptr_t)ph[i].p_memsz;
		uintptr_t seg_phys = phys_base + (aligned_vaddr - base_vaddr);

		uint64_t flags = VMM_PRESENT;
		if (ph[i].p_flags & PF_W)
			flags |= VMM_WRITABLE;
		if (!(ph[i].p_flags & PF_X))
			flags |= VMM_NX;

		map_pages(pagemap, aligned_vaddr, seg_phys, seg_size, flags);
		memcpy((void *)PHYS_TO_VIRT(seg_phys + seg_off), data + ph[i].p_offset,
			   ph[i].p_filesz);

		if (ph[i].p_filesz < ph[i].p_memsz) {
			memset((void *)PHYS_TO_VIRT(seg_phys + seg_off + ph[i].p_filesz), 0,
				   ph[i].p_memsz - ph[i].p_filesz);
		}
	}

	elf64_apply_relocations_ex(header, phys_base, base_vaddr, base_vaddr);

	uintptr_t entry_addr = (uintptr_t)header->e_entry;
	if (header->e_type == ET_DYN) {
		entry_addr = base_vaddr + (uintptr_t)header->e_entry;
	}

	debug("ELF loaded successfully, entry point: 0x%llx\n", entry_addr);
	return entry_addr;
}

bool elf_get_load_range(char *data, uintptr_t *link_base_out, size_t *size_out)
{
	if (!data || !link_base_out || !size_out)
		return false;

	Elf64_Ehdr *header = (Elf64_Ehdr *)data;
	if (header->e_ident[EI_MAG0] != ELFMAG0 ||
		header->e_ident[EI_MAG1] != ELFMAG1 ||
		header->e_ident[EI_MAG2] != ELFMAG2 ||
		header->e_ident[EI_MAG3] != ELFMAG3)
		return false;
	if (header->e_ident[EI_CLASS] != ELFCLASS64)
		return false;
	if (header->e_machine != EM_AMD64)
		return false;

	Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)data + header->e_phoff);
	uintptr_t link_base = (uintptr_t)-1;
	uintptr_t end_vaddr = 0;

	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0)
			continue;

		uintptr_t seg_start = ALIGN_DOWN((uintptr_t)ph[i].p_vaddr, PAGE_SIZE);
		uintptr_t seg_end = ALIGN_UP(
			(uintptr_t)ph[i].p_vaddr + (uintptr_t)ph[i].p_memsz, PAGE_SIZE);

		if (seg_start < link_base)
			link_base = seg_start;
		if (seg_end > end_vaddr)
			end_vaddr = seg_end;
	}

	if (link_base == (uintptr_t)-1 || end_vaddr <= link_base)
		return false;

	*link_base_out = link_base;
	*size_out = (size_t)(end_vaddr - link_base);
	return true;
}

bool elf_load_image_at(char *data, pagetable *pagemap, uintptr_t load_base,
					   elf_loaded_image_t *out)
{
	if (!data || !pagemap || !out)
		return false;

	Elf64_Ehdr *header = (Elf64_Ehdr *)data;
	if (header->e_ident[EI_MAG0] != ELFMAG0 ||
		header->e_ident[EI_MAG1] != ELFMAG1 ||
		header->e_ident[EI_MAG2] != ELFMAG2 ||
		header->e_ident[EI_MAG3] != ELFMAG3) {
		error("elf_load_image_at(): Invalid ELF magic\n");
		return false;
	}
	if (header->e_ident[EI_CLASS] != ELFCLASS64 ||
		header->e_machine != EM_AMD64) {
		error("elf_load_image_at(): Unsupported ELF\n");
		return false;
	}
	if (header->e_type != ET_DYN) {
		error("elf_load_image_at(): Expected ET_DYN PIE\n");
		return false;
	}

	uintptr_t link_base = 0;
	size_t exec_size = 0;
	if (!elf_get_load_range(data, &link_base, &exec_size)) {
		error("elf_load_image_at(): No loadable segments\n");
		return false;
	}

	exec_size = (size_t)ALIGN_UP(exec_size, PAGE_SIZE);
	size_t pages = exec_size / PAGE_SIZE;
	uintptr_t phys_base = (uintptr_t)palloc(pages);
	if (!phys_base) {
		error("elf_load_image_at(): OOM\n");
		return false;
	}

	return elf_load_image_mapped(data, pagemap, load_base, phys_base, out);
}

bool elf_load_image_mapped(char *data, pagetable *pagemap, uintptr_t load_base,
						   uintptr_t phys_base, elf_loaded_image_t *out)
{
	if (!data || !pagemap || !out)
		return false;

	Elf64_Ehdr *header = (Elf64_Ehdr *)data;
	if (header->e_ident[EI_MAG0] != ELFMAG0 ||
		header->e_ident[EI_MAG1] != ELFMAG1 ||
		header->e_ident[EI_MAG2] != ELFMAG2 ||
		header->e_ident[EI_MAG3] != ELFMAG3) {
		error("elf_load_image_mapped(): Invalid ELF magic\n");
		return false;
	}
	if (header->e_ident[EI_CLASS] != ELFCLASS64 ||
		header->e_machine != EM_AMD64) {
		error("elf_load_image_mapped(): Unsupported ELF\n");
		return false;
	}
	if (header->e_type != ET_DYN) {
		error("elf_load_image_mapped(): Expected ET_DYN PIE\n");
		return false;
	}

	uintptr_t link_base = 0;
	size_t exec_size = 0;
	if (!elf_get_load_range(data, &link_base, &exec_size)) {
		error("elf_load_image_mapped(): No loadable segments\n");
		return false;
	}

	exec_size = (size_t)ALIGN_UP(exec_size, PAGE_SIZE);

	memset((void *)PHYS_TO_VIRT(phys_base), 0, exec_size);

	Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)data + header->e_phoff);
	for (uint16_t i = 0; i < header->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0)
			continue;

		uintptr_t aligned_vaddr =
			ALIGN_DOWN((uintptr_t)ph[i].p_vaddr, PAGE_SIZE);
		uintptr_t seg_off = (uintptr_t)ph[i].p_vaddr - aligned_vaddr;
		uintptr_t seg_size = seg_off + (uintptr_t)ph[i].p_memsz;
		uintptr_t seg_phys = phys_base + (aligned_vaddr - link_base);
		uintptr_t seg_virt = load_base + (aligned_vaddr - link_base);

		uint64_t flags = VMM_PRESENT;
		if (ph[i].p_flags & PF_W)
			flags |= VMM_WRITABLE;
		if (!(ph[i].p_flags & PF_X))
			flags |= VMM_NX;

		/* Update permissions for this segment. */
		map_pages(pagemap, seg_virt, seg_phys, seg_size, flags);

		memcpy((void *)PHYS_TO_VIRT(seg_phys + seg_off), data + ph[i].p_offset,
			   ph[i].p_filesz);
		if (ph[i].p_filesz < ph[i].p_memsz) {
			memset((void *)PHYS_TO_VIRT(seg_phys + seg_off + ph[i].p_filesz), 0,
				   ph[i].p_memsz - ph[i].p_filesz);
		}
	}

	elf64_apply_relocations_ex(header, phys_base, load_base, link_base);

	out->phys_base = phys_base;
	out->load_base = load_base;
	out->link_base = link_base;
	out->size = exec_size;
	out->entry = load_base + ((uintptr_t)header->e_entry - link_base);
	return true;
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

static bool elf64_get_rela_symtab(Elf64_Ehdr *header, Elf64_Shdr *rela_sh,
								  Elf64_Sym **symtab_out, size_t *nsyms_out,
								  const char **strtab_out)
{
	if (!header || !rela_sh || !symtab_out || !nsyms_out || !strtab_out)
		return false;
	if (header->e_shoff == 0 || header->e_shnum == 0)
		return false;

	Elf64_Shdr *shdr = (Elf64_Shdr *)((uint8_t *)header + header->e_shoff);
	if (rela_sh->sh_link >= header->e_shnum)
		return false;
	Elf64_Shdr *symtab_sh = &shdr[rela_sh->sh_link];
	if (symtab_sh->sh_size == 0)
		return false;
	if (symtab_sh->sh_type != SHT_SYMTAB && symtab_sh->sh_type != SHT_DYNSYM)
		return false;
	if (symtab_sh->sh_link >= header->e_shnum)
		return false;
	Elf64_Shdr *strtab_sh = &shdr[symtab_sh->sh_link];
	if (strtab_sh->sh_type != SHT_STRTAB || strtab_sh->sh_size == 0)
		return false;

	*symtab_out = (Elf64_Sym *)((uint8_t *)header + symtab_sh->sh_offset);
	*nsyms_out = symtab_sh->sh_size / sizeof(Elf64_Sym);
	*strtab_out = (const char *)header + strtab_sh->sh_offset;
	return true;
}

static void elf64_apply_relocations_ex(Elf64_Ehdr *header, uintptr_t phys_base,
									   uintptr_t load_base, uintptr_t link_base)
{
	if (!header || header->e_shoff == 0 || header->e_shnum == 0)
		return;

	if (header->e_type != ET_DYN)
		return;

	Elf64_Shdr *shdr = (Elf64_Shdr *)((uint8_t *)header + header->e_shoff);

	for (Elf64_Half i = 0; i < header->e_shnum; i++) {
		if (shdr[i].sh_type != SHT_RELA || shdr[i].sh_size == 0)
			continue;

		Elf64_Sym *symtab = NULL;
		size_t nsyms = 0;
		const char *strtab = NULL;
		(void)elf64_get_rela_symtab(header, &shdr[i], &symtab, &nsyms, &strtab);

		Elf64_Rela *rela =
			(Elf64_Rela *)((uint8_t *)header + shdr[i].sh_offset);
		size_t count = shdr[i].sh_size / sizeof(Elf64_Rela);

		for (size_t j = 0; j < count; j++) {
			Elf64_Word type = ELF64_R_TYPE(rela[j].r_info);
			Elf64_Word sym_idx = (Elf64_Word)ELF64_R_SYM(rela[j].r_info);

			uintptr_t where_link = (uintptr_t)rela[j].r_offset;
			if (where_link < link_base)
				continue;
			uintptr_t where_off = where_link - link_base;
			uint8_t *where_ptr = (uint8_t *)PHYS_TO_VIRT(phys_base + where_off);
			uintptr_t P = load_base + where_off;
			uintptr_t S = 0;
			intptr_t A = (intptr_t)rela[j].r_addend;

			if (type != R_X86_64_RELATIVE && symtab && strtab &&
				sym_idx < nsyms) {
				Elf64_Sym *sym = &symtab[sym_idx];
				const char *name =
					sym->st_name ? (strtab + sym->st_name) : NULL;
				if (sym->st_shndx != SHN_UNDEF) {
					if ((uintptr_t)sym->st_value >= link_base)
						S = load_base + ((uintptr_t)sym->st_value - link_base);
				} else if (name && name[0] != '\0') {
					S = axapi_resolve(name);
					if (!S && ELF64_ST_BIND(sym->st_info) == STB_WEAK)
						S = 0;
				}
			}

			if (type == R_X86_64_RELATIVE) {
				*(uint64_t *)where_ptr = (uint64_t)(load_base + (uintptr_t)A);
			} else if (type == R_X86_64_64) {
				*(uint64_t *)where_ptr = (uint64_t)(S + (uintptr_t)A);
			} else if (type == R_X86_64_GLOB_DAT ||
					   type == R_X86_64_JUMP_SLOT) {
				*(uint64_t *)where_ptr = (uint64_t)S;
			} else if (type == R_X86_64_PC32 || type == R_X86_64_PLT32) {
				int64_t val = (int64_t)((S + (uintptr_t)A) - P);
				if (val < INT32_MIN || val > INT32_MAX) {
					debug("Reloc overflow (PC32) S=%p P=%p\n", S, P);
					continue;
				}
				*(int32_t *)where_ptr = (int32_t)val;
			} else if (type == R_X86_64_32) {
				uint64_t val = (uint64_t)(S + (uintptr_t)A);
				*(uint32_t *)where_ptr = (uint32_t)val;
			} else if (type == R_X86_64_32S) {
				int64_t val = (int64_t)((uint64_t)(S + (uintptr_t)A));
				if (val < INT32_MIN || val > INT32_MAX) {
					debug("Reloc overflow (32S)\n");
					continue;
				}
				*(int32_t *)where_ptr = (int32_t)val;
			} else {
				debug("Unsupported reloc type %u\n", type);
			}
		}
	}
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

bool elf_lookup_addr(char *elf_data, uintptr_t addr, const char **name_out,
					 uintptr_t *sym_addr_out)
{
	if (!name_out)
		return false;
	*name_out = NULL;
	if (sym_addr_out)
		*sym_addr_out = 0;

	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr->e_ident[EI_CLASS] != ELFCLASS64 || ehdr->e_machine != EM_AMD64) {
		return false;
	}

	Elf64_Sym *symtab = NULL;
	size_t nsyms = 0;
	const char *strtab = NULL;
	if (!elf64_get_symtab(ehdr, &symtab, &nsyms, &strtab))
		return false;

	const Elf64_Sym *best = NULL;
	for (size_t i = 0; i < nsyms; i++) {
		const Elf64_Sym *s = &symtab[i];
		if (s->st_shndx == SHN_UNDEF)
			continue;
		if (ELF64_ST_TYPE(s->st_info) != STT_FUNC)
			continue;
		uintptr_t s_addr = (uintptr_t)s->st_value;
		if (s_addr == 0 || s_addr > addr)
			continue;
		if (!best || s_addr > (uintptr_t)best->st_value)
			best = s;
	}

	if (!best)
		return false;

	*name_out = strtab + best->st_name;
	if (sym_addr_out)
		*sym_addr_out = (uintptr_t)best->st_value;
	return true;
}
