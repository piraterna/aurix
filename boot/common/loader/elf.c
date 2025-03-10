/*********************************************************************************/
/* Module Name:  elf.c                                                           */
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

#include <loader/elf.h>
#include <mm/vmm.h>
#include <mm/mman.h>
#include <lib/string.h>
#include <lib/align.h>
#include <print.h>
#include <stdint.h>

/* https://github.com/KevinAlavik/nekonix/blob/main/kernel/src/proc/elf.c */
/* Thanks, Kevin <3 */

uintptr_t elf32_load(char *data, pagetable *pagemap)
{
    (void)data;
    (void)pagemap;
    return 0;
}

uintptr_t elf64_load(char *data, pagetable *pagemap)
{
    struct elf_header *header = (struct elf_header *)data;
    struct elf_program_header *ph = (struct elf_program_header *)((uint8_t *)data + header->e_phoff);

    uint64_t lowest = UINT64_MAX;
    uint64_t max_align = 0;

    for (uint16_t i = 0; i < header->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD)
            continue;
        
        if (ph[i].p_align > max_align) {
            max_align = ph[i].p_align;
        }
    }

    for (uint16_t i = 0; i < header->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD)
            continue;
        
        if ((ph[i].p_vaddr & (~(max_align - 1))) < lowest) {
            lowest = ph[i].p_vaddr & ~(max_align - 1);
        }

        uint64_t flags = VMM_PRESENT;
        if (ph[i].p_flags & PF_W)
            flags |= VMM_WRITABLE;
        if (!(ph[i].p_flags & PF_X))
            flags |= VMM_NX;
        
        debug("elf64_load(): phys=0x%llx, virt=0x%llx, size=%lu\n", ph[i].p_paddr, ph[i].p_vaddr, ph[i].p_filesz);
        
        uint64_t phys = (uint64_t)mem_alloc(ph[i].p_memsz);
        if (!phys) {
            debug("elf64_load(): Out of memory\n");
            return 0;
        }

        map_page(pagemap, ph[i].p_vaddr, phys, flags);
        memcpy((void*)ph[i].p_vaddr - lowest, data + ph[i].p_offset, ph[i].p_filesz);
    }

    debug("elf64_load(): ELF loaded successfully, entry: 0x%llx\n", header->e_entry);
    return (uintptr_t)((uint8_t *)data + (header->e_entry - lowest));
}

uintptr_t elf_load(char *data, pagetable *pagemap)
{
    struct elf_header *header = (struct elf_header *)data;

    if (header->e_magic != ELF_MAGIC) {
        debug("Invalid ELF magic: 0x%x", header->e_magic);
        return 0;
    }

    if (header->e_class != 2) {
        debug("Unsupported ELF class: %u", header->e_class);
        return 0;
    }

    if (header->e_machine == 20 ||
        header->e_machine == 3 ||
        header->e_machine == 40) {
        return elf32_load(data, pagemap);
    } else if (header->e_machine == 62) {
        return elf64_load(data, pagemap);
    }

    debug("Unsupported ELF machine: %u", header->e_machine);
    return 0;
}
