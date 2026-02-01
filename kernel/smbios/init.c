/*********************************************************************************/
/* Module Name:  init.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <smbios/smbios.h>
#include <mm/vmm.h>
#include <aurix.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

char *smbios_get_str(void *addr, uint8_t index)
{
	char *paddr = addr;

	for (int i = 0; i < (index - 1); i++) {
		paddr += strlen(paddr) + 1;
	}

	return paddr;
}

size_t smbios_struct_len(struct smbios_hdr *hdr)
{
	size_t i;
	char *sp = (char *)hdr + hdr->len;

	while (*sp != '\0' && *(sp - 1) != '\0') {
		sp++;
		i++;
	}

	return i;
}

void smbios_init(void *addr)
{
	if (!addr) {
		warn("No SMBIOS EP struct address specified!\n");
		return;
	}

	map_page(NULL, PHYS_TO_VIRT(addr), (uintptr_t)addr, VMM_PRESENT | VMM_NX);

	info("SMBIOS Entry Point: 0x%llx\n", addr);

	char *ep_ptr = (char *)PHYS_TO_VIRT(addr);
	if (strncmp(ep_ptr, "_SM3_", 5) == 0) {
		info("SMBIOS v3.x\n");
	} else if (strncmp(ep_ptr, "_SM_", 4) == 0) {
		info("SMBIOS v2.x\n");
	} else {
		error("Unknown SMBIOS version!\n");
		return;
	}
}