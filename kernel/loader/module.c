/*********************************************************************************/
/* Module Name:  module.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <loader/module.h>
#include <loader/elf.h>
#include <sys/sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <debug/log.h>
#include <sys/aurix/mod.h>
#include <sys/axapi.h>
#include <aurix/axapi.h>
#include <lib/string.h>
#include <lib/align.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <arch/cpu/cpu.h>
#include <arch/mm/paging.h>

pcb **loaded_modules = (pcb **)NULL;

#define MODULE_VA_BASE 0xffffffffe0000000ULL

static vctx_t *kmod_vctx = NULL;

struct module_image {
	struct module_image *next;
	char *elf;
	uintptr_t load_base;
	uintptr_t link_base;
	size_t exec_size;
};

static struct module_image *module_images = NULL;
static struct module_info_node *module_list = NULL;

bool module_lookup_image(uintptr_t addr, char **elf_out,
						 uintptr_t *load_base_out, uintptr_t *link_base_out)
{
	if (!elf_out || !load_base_out || !link_base_out)
		return false;
	*elf_out = NULL;
	*load_base_out = 0;
	*link_base_out = 0;

	for (struct module_image *m = module_images; m; m = m->next) {
		if (addr < m->load_base)
			continue;
		if (addr >= m->load_base + m->exec_size)
			continue;
		*elf_out = m->elf;
		*load_base_out = m->load_base;
		*link_base_out = m->link_base;
		return true;
	}

	return false;
}

static Elf64_Shdr *elf64_find_section(Elf64_Ehdr *ehdr, const char *name)
{
	Elf64_Shdr *shdr = (Elf64_Shdr *)((uint8_t *)ehdr + ehdr->e_shoff);
	Elf64_Shdr *shstrtab = &shdr[ehdr->e_shstrndx];
	const char *shstrtab_data = (const char *)ehdr + shstrtab->sh_offset;

	for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
		const char *sec_name = shstrtab_data + shdr[i].sh_name;
		if (strcmp(sec_name, name) == 0)
			return &shdr[i];
	}

	return NULL;
}

static uintptr_t elf64_lookup_symbol_in_sections(Elf64_Ehdr *ehdr,
												 const char *symtab_name,
												 const char *strtab_name,
												 const char *name)
{
	Elf64_Shdr *symtab = elf64_find_section(ehdr, symtab_name);
	Elf64_Shdr *strtab = elf64_find_section(ehdr, strtab_name);

	if (!symtab || !strtab)
		return 0;

	if (symtab->sh_entsize == 0 || symtab->sh_size < symtab->sh_entsize)
		return 0;

	const char *strs = (const char *)ehdr + strtab->sh_offset;
	Elf64_Sym *syms = (Elf64_Sym *)((uint8_t *)ehdr + symtab->sh_offset);
	size_t count = symtab->sh_size / sizeof(Elf64_Sym);

	for (size_t i = 0; i < count; i++) {
		const char *symname = strs + syms[i].st_name;
		if (strcmp(symname, name) == 0)
			return syms[i].st_value;
	}

	return 0;
}

static uintptr_t elf64_lookup_symbol_any(char *elf, const char *name)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
	uintptr_t addr;

	addr = elf64_lookup_symbol_in_sections(ehdr, ".symtab", ".strtab", name);
	if (addr)
		return addr;

	addr = elf64_lookup_symbol_in_sections(ehdr, ".dynsym", ".dynstr", name);
	if (addr)
		return addr;

	return 0;
}

static struct axmod_info *module_find_modinfo_section(char *elf,
													  uintptr_t load_base,
													  uintptr_t link_base)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
	Elf64_Shdr *sec = elf64_find_section(ehdr, ".aurix.mod");
	if (!sec)
		return NULL;

	if (sec->sh_size < sizeof(struct axmod_info)) {
		warn("'.aurix.mod' section too small (%lu bytes)\n",
			 (unsigned long)sec->sh_size);
		return NULL;
	}

	if (sec->sh_addr < link_base) {
		warn("'.aurix.mod' below link base\n");
		return NULL;
	}

	return (struct axmod_info *)(load_base + (sec->sh_addr - link_base));
}

static struct axmod_info *module_find_modinfo(char *elf,
											  uintptr_t load_base,
											  uintptr_t link_base)
{
	struct axmod_info *mi;

	/* Primary path: metadata section */
	mi = module_find_modinfo_section(elf, load_base, link_base);
	if (mi)
		return mi;

	/* Fallback: symbol lookup in .symtab or .dynsym */
	uintptr_t modinfo_addr = elf64_lookup_symbol_any(elf, "modinfo");
	if (!modinfo_addr)
		return NULL;

	if (modinfo_addr < link_base) {
		warn("'modinfo' below link base\n");
		return NULL;
	}

	return (struct axmod_info *)(load_base + (modinfo_addr - link_base));
}

/* ======================================== */
/* axapi patching                           */
/* ======================================== */

static void axapi_patch_imports(uintptr_t load_base, uintptr_t link_base,
								char *elf)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
	Elf64_Shdr *imports = elf64_find_section(ehdr, ".axapi.imports");
	if (!imports || imports->sh_size == 0)
		return;

	size_t count = imports->sh_size / sizeof(struct axapi_import);
	struct axapi_import *imp =
		(struct axapi_import *)(load_base +
								((uintptr_t)imports->sh_addr - link_base));

	for (size_t i = 0; i < count; i++) {
		const char *name = imp[i].name;
		uintptr_t resolved = axapi_resolve(name);
		if (!resolved) {
			error("Unresolved AXAPI import: %s\n", name ? name : "(null)");
			continue;
		}

		if (!imp[i].target) {
			error("AXAPI import target NULL: %s\n", name ? name : "(null)");
			continue;
		}
		*imp[i].target = resolved;
	}
}

static int axmod_get_current_cpuid(void)
{
	struct cpu *c = cpu_get_current();
	return c ? (int)c->id : 0;
}

bool module_load_image(void *image, uint32_t size)
{
	uint32_t file_size = size;
	uintptr_t entry_point = 0;
	pcb *mod = proc_create();

	if (!mod) {
		error("Failed to create process for module\n");
		return false;
	}

	char *virt_data = (char *)image;
	mod->image_elf = virt_data;
	mod->image_size = file_size;

	if (!kmod_vctx) {
		kmod_vctx = vinit(kernel_pm, MODULE_VA_BASE);
		if (!kmod_vctx) {
			error("Failed to init module vctx\n");
			return false;
		}
	}

	uintptr_t link_base = 0;
	size_t exec_size = 0;
	if (!elf_get_load_range(virt_data, &link_base, &exec_size)) {
		error("Failed to query module load range\n");
		return false;
	}

	exec_size = (size_t)ALIGN_UP(exec_size, PAGE_SIZE);
	size_t pages = exec_size / PAGE_SIZE;
	uintptr_t phys_base = (uintptr_t)palloc(pages);
	if (!phys_base) {
		error("Failed to allocate module physical memory\n");
		return false;
	}

	uintptr_t load_base =
		(uintptr_t)vallocatp(kmod_vctx, pages, VALLOC_RW, (uint64_t)phys_base);
	if (!load_base) {
		error("Failed to allocate module virtual range\n");
		pfree((void *)phys_base, pages);
		return false;
	}

	elf_loaded_image_t img;
	memset(&img, 0, sizeof(img));
	if (!elf_load_image_mapped(virt_data, kernel_pm, load_base, phys_base, &img,
							   false, true)) {
		error("Failed to load module file\n");
		vfree(kmod_vctx, (void *)load_base);
		pfree((void *)phys_base, pages);
		return false;
	}
	entry_point = img.entry;
	mod->image_phys_base = img.phys_base;
	mod->image_load_base = img.load_base;
	mod->image_link_base = img.link_base;
	mod->image_exec_size = img.size;

	struct axmod_info *mi = module_find_modinfo(virt_data,
												img.load_base,
												img.link_base);
	uintptr_t init_vaddr = 0;

	if (mi) {
		const char *name = mi->name;
		const char *desc = mi->desc;
		const char *author = mi->author;

		mod->name = name;
		struct module_info_node *node = kmalloc(sizeof(*node));
		if (node) {
			node->proc = mod;
			node->name = name;
			node->desc = desc;
			node->author = author;
			node->init = mi->mod_init;
			node->exit = mi->mod_exit;
			node->load_base = img.load_base;

			node->next = module_list;
			module_list = node;
		}

		info("Loaded module: %s\n", name ? name : "(no name)");
		if (desc)
			info("  Description: %s\n", desc);
		if (author)
			info("  Author: %s\n", author);

		if (mi->mod_init) {
			init_vaddr = (uintptr_t)mi->mod_init;
			trace("  Init function: %p\n", mi->mod_init);
		} else {
			warn("Module has no mod_init callback\n");
		}

		if (mi->mod_exit)
			trace("  Exit function: %p\n", mi->mod_exit);
	} else {
		warn("No module metadata found (.aurix.mod/.symtab/.dynsym)\n");
	}

	axapi_patch_imports(img.load_base, img.link_base, virt_data);
	thread_create(mod, (void (*)())(void *)init_vaddr);

	struct module_image *rec = kmalloc(sizeof(*rec));
	if (rec) {
		rec->elf = virt_data;
		rec->load_base = img.load_base;
		rec->link_base = img.link_base;
		rec->exec_size = img.size;
		rec->next = module_images;
		module_images = rec;
	}

	trace("Module loaded at phys 0x%lx, vaddr 0x%lx, entry 0x%lx\n",
		  img.phys_base, img.load_base, entry_point);

	return true;
}

bool module_load(void *addr, uint32_t size)
{
	if (!addr) {
		return false;
	}

	return module_load_image((void *)PHYS_TO_VIRT(addr), size);
}

struct module_info_node *module_get_list(void)
{
	return module_list;
}
