/*********************************************************************************/
/* Module Name:  acpi.c */
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

#ifndef _ACPI_ACPI_H
#define _ACPI_ACPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct sdt_header {
	char sig[4];
	uint32_t len;
	uint8_t revision;
	uint8_t csum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__((packed));

struct rsdp {
	char sig[8];
	uint8_t csum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_addr;
} __attribute__((packed));

struct xsdp {
	struct rsdp rsdp;
	uint32_t len;
	uint64_t xsdt_addr;
	uint8_t ext_csum;
	uint8_t reserved[3];
} __attribute__((packed));

struct rsdt {
	struct sdt_header hdr;
	uint32_t sdt_ptr[];
} __attribute__((packed));

struct xsdt {
	struct sdt_header hdr;
	uint64_t sdt_ptr[];
} __attribute__((packed));

bool acpi_init(void *rsdp_addr);
void *find_sdt(char *sig);

#endif /* _ACPI_ACPI_H */