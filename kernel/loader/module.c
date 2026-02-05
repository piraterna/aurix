#include <loader/module.h>
#include <loader/elf.h>
#include <sys/sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <debug/log.h>
#include <sys/aurix/mod.h>

pcb **loaded_modules = (pcb **)NULL;

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

	struct axmod_info *modinfo = NULL;

	if (modinfo_addr != 0) {
		modinfo = (struct axmod_info *)modinfo_addr;

		info("Loaded module: %s\n",
			 modinfo->name ? modinfo->name : "(no name)");
		if (modinfo->desc) {
			info("  Description: %s\n", modinfo->desc);
		}
		if (modinfo->author) {
			info("  Author: %s\n", modinfo->author);
		}
		if (modinfo->mod_init) {
			info("  Init function: %p\n", modinfo->mod_init);
		}
		if (modinfo->mod_exit) {
			info("  Exit function: %p\n", modinfo->mod_exit);
		}
	} else {
		warn("No 'modinfo' symbol found in module\n");
	}

	thread_create(mod, (void (*)())entry_point);

	info("Module loaded at physical 0x%lx, entry point 0x%lx\n", loaded_at,
		 entry_point);

	return true;
}