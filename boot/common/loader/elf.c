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
    struct elf_program_header *ph = (struct elf_program_header *)((uint8_t *)data + header->e_phoff);

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
        
        uint64_t aligned_vaddr = ph[i].p_vaddr & ~(max_align - 1);

        uint64_t flags = VMM_PRESENT;
        if (ph[i].p_flags & PF_W)
            flags |= VMM_WRITABLE;
        if (!(ph[i].p_flags & PF_X))
            flags |= VMM_NX;
    
        uint64_t phys = ((uint64_t)mem_alloc(ph[i].p_memsz + ph[i].p_vaddr - aligned_vaddr + 4096) + 4096) & ~0xFFF;
        if (!phys) {
            log("elf64_load(): Out of memory\n");
            return 0;
        }

        if (addr != NULL && *addr == 0) {
            *addr = phys;
        }
        
        debug("elf64_load(): phys=0x%llx, virt=0x%llx, size=%lu\n", phys, ph[i].p_vaddr, ph[i].p_filesz);

        map_page(pagemap, aligned_vaddr, phys, flags);
        memcpy((void*)(phys + ph[i].p_vaddr - aligned_vaddr), data + ph[i].p_offset, ph[i].p_filesz);
    }

    log("elf64_load(): ELF loaded successfully, entry: 0x%llx\n", header->e_entry);
    return (uintptr_t)header->e_entry;
}

uintptr_t elf_load(char *data, uintptr_t *addr, pagetable *pagemap)
{
    struct elf_header *header = (struct elf_header *)data;

    if (header->e_magic != ELF_MAGIC) {
        log("elf_load(): Invalid ELF magic: 0x%x", header->e_magic);
        return 0;
    }

    if (header->e_class != 2) {
        log("elf_load(): Unsupported ELF class: %u", header->e_class);
        return 0;
    }

    if (header->e_machine == 20 ||
        header->e_machine == 3 ||
        header->e_machine == 40) {
        return elf32_load(data, addr, pagemap);
    } else if (header->e_machine == 62) {
        return elf64_load(data, addr, pagemap);
    }

    log("elf_load(): Unsupported ELF machine: %u", header->e_machine);
    return 0;
}
