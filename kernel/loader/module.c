#include <loader/module.h>
#include <loader/elf.h>
#include <sys/sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <debug/log.h>
#include <sys/aurix/mod.h>
#include <string.h>
#include <mm/pmm.h>

pcb **loaded_modules = (pcb **)NULL;

static void *elf64_vaddr_to_file_ptr(char *elf, uintptr_t vaddr)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
		return NULL;
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS64)
		return NULL;

	Elf64_Phdr *ph = (Elf64_Phdr *)(elf + ehdr->e_phoff);
	for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD)
			continue;
		uintptr_t start = (uintptr_t)ph[i].p_vaddr;
		uintptr_t end = start + (uintptr_t)ph[i].p_filesz;
		if (vaddr >= start && vaddr < end) {
			uintptr_t off = (uintptr_t)ph[i].p_offset + (vaddr - start);
			return elf + off;
		}
	}

	return NULL;
}

static const char *elf64_vaddr_to_file_cstr(char *elf, uintptr_t vaddr)
{
	return (const char *)elf64_vaddr_to_file_ptr(elf, vaddr);
}

bool module_load(void *addr, uint32_t size)
{
	uintptr_t entry_point = 0;
	uintptr_t loaded_at = 0;
	pcb *mod = proc_create();

	if (!mod) {
		error("Failed to create process for module\n");
		return false;
	}

	char *virt_data = (char *)PHYS_TO_VIRT(addr);

	entry_point = elf_load(virt_data, &loaded_at, (size_t *)&size, mod->pm);
	if (entry_point == 0) {
		error("Failed to load module file\n");
		return false;
	}

	uintptr_t modinfo_addr = elf_lookup_symbol(virt_data, "modinfo");
	uintptr_t init_vaddr = entry_point;

	if (modinfo_addr != 0) {
		void *mi_ptr = elf64_vaddr_to_file_ptr(virt_data, modinfo_addr);
		if (mi_ptr) {
			struct axmod_info mi;
			memcpy(&mi, mi_ptr, sizeof(mi));

			const char *name =
				mi.name ?
					elf64_vaddr_to_file_cstr(virt_data, (uintptr_t)mi.name) :
					NULL;
			const char *desc =
				mi.desc ?
					elf64_vaddr_to_file_cstr(virt_data, (uintptr_t)mi.desc) :
					NULL;
			const char *author =
				mi.author ?
					elf64_vaddr_to_file_cstr(virt_data, (uintptr_t)mi.author) :
					NULL;

			info("Loaded module: %s\n", name ? name : "(no name)");
			if (desc)
				info("  Description: %s\n", desc);
			if (author)
				info("  Author: %s\n", author);
			if (mi.mod_init) {
				init_vaddr = (uintptr_t)mi.mod_init;
				info("  Init function: %p\n", mi.mod_init);
			}
			if (mi.mod_exit)
				info("  Exit function: %p\n", mi.mod_exit);
		} else {
			warn("'modinfo' not in any loadable segment\n");
		}
	} else {
		warn("No 'modinfo' symbol found in module\n");
	}

	void *exports_phys = palloc(1);
	if (!exports_phys) {
		error("Failed to allocate module exports page\n");
		return false;
	}
	map_page(mod->pm, AXMOD_EXPORTS_VADDR, (uintptr_t)exports_phys,
			 VMM_PRESENT | VMM_WRITABLE | VMM_NX);
	struct axmod_exports *exports =
		(struct axmod_exports *)PHYS_TO_VIRT(exports_phys);
	memset(exports, 0, sizeof(*exports));
	exports->kprintf = kprintf;
	exports->sched_yield = sched_yield;

	thread_create(mod, (void (*)())(void *)init_vaddr);

	info("Module loaded at physical 0x%lx, entry point 0x%lx\n", loaded_at,
		 entry_point);

	return true;
}
