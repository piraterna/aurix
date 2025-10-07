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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#include <acpi/acpi.h>
#include <acpi/madt.h>
#include <boot/aurix.h>
#include <debug/print.h>
#include <lib/string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool is_xsdt = false;
struct rsdt *rsdt = NULL;
struct xsdt *xsdt = NULL;

static bool checksum(struct sdt_header *hdr)
{
	uint8_t checksum = 0;
	void *hdr_ptr = (void *)hdr;
	for (size_t i = 0; i < hdr->len; i++) {
		checksum += *((uint8_t *)hdr_ptr);
		hdr_ptr++;
	}

	return checksum == 0;
}

bool acpi_init(void *rsdp_addr)
{
	uint8_t checksum = 0;
	void *rsdp_ptr = (void *)((uintptr_t)rsdp_addr + boot_params->hhdm_offset);
	struct xsdp *rsdp = (struct xsdp *)((uintptr_t)rsdp_addr + boot_params->hhdm_offset);
	is_xsdt = rsdp->rsdp.revision >= 2;

	for (size_t i = 0; i < (is_xsdt ? sizeof(struct xsdp) : sizeof(struct rsdp)); i++) {
		checksum += *((uint8_t *)rsdp_ptr);
		rsdp_ptr++;
	}

	if (checksum != 0) {
		klog("Invalid RSDP checksum!\n");
	}

	if (is_xsdt) {
		xsdt = (struct xsdt *)(rsdp->xsdt_addr + boot_params->hhdm_offset);
	} else {
		rsdt = (struct rsdt *)(rsdp->rsdp.rsdt_addr + boot_params->hhdm_offset);
	}

	// parse the almighty SDTs!
	acpi_madt_init();

	return true;
}

void *find_sdt(char *sig, size_t len)
{
	size_t sdt_len = is_xsdt ? (xsdt->hdr.len - sizeof(struct sdt_header) / 8) : (rsdt->hdr.len - sizeof(struct sdt_header) / 4);
	struct sdt_header *hdr;
	for (size_t i = 0; i < sdt_len; i++) {
		if (is_xsdt) {
			hdr = (struct sdt_header *)(xsdt->sdt_ptr[i] + boot_params->hhdm_offset);
		} else {
			hdr = (struct sdt_header *)(rsdt->sdt_ptr[i] + boot_params->hhdm_offset);
		}

		if (strncmp(hdr->sig, sig, len) == 0) {
			if (checksum(hdr)) {
				return (void *)((uintptr_t)hdr + boot_params->hhdm_offset);
			}
		}
	}

	return NULL;
}