#include <loader/module.h>
#include <loader/elf.h>
#include <sys/sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

pcb **loaded_modules = (pcb **)NULL;

bool module_load(void *addr, uint32_t size)
{
	uintptr_t entry_point = 0;
	uintptr_t loaded_at = 0;
	pcb *mod = proc_create();

	addr = (void *)PHYS_TO_VIRT(addr);

	entry_point = elf_load(addr, &loaded_at, (size_t *)&size, mod->pm);
	thread_create(mod, (void (*)())entry_point);

	return true;
}