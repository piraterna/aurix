/*********************************************************************************/
/* Module Name:  madt.c */
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

#ifndef _ACPI_MADT_H
#define _ACPI_MADT_H

#include <acpi/acpi.h>
#include <stdint.h>

#define MADT_LAPIC 0x00
#define MADT_IOAPIC 0x01
#define MADT_ISO 0x02
#define MADT_NMI_SRC 0x03
#define MADT_LAPIC_NMI 0x04
#define MADT_LAPIC_NMI_OVERRIDE 0x05
#define MADT_LX2APIC 0x09
#define MADT_LX2APIC_NMI 0x0a

#define MADT_GICC 0x0b
#define MADT_GICD 0x0c
#define MADT_GIC_MSI_FRAME 0x0d
#define MADT_GICR 0x0e
#define MADT_GIC_ITS 0x0f

struct madt_header {
	uint8_t type;
	uint8_t len;
} __attribute__((packed));

#if defined(__x86_64__)
enum mps_inti_flags {
	POLARITY_BUS = 0,
	POLARITY_ACTIVE_HI = 1,
	POLARITY_ACTIVE_LO = 3,

	TRIGGER_BUS = 0,
	TRIGGER_EDGE = 1,
	TRIGGER_LEVEL = 3
};

struct madt_lapic {
	struct madt_header hdr;
	uint8_t uid;
	uint8_t id;
	uint32_t flags;
} __attribute__((packed));

struct madt_ioapic {
	struct madt_header hdr;
	uint8_t id;
	uint8_t reserved;
	uint32_t addr;
	uint32_t gsi_base;
} __attribute__((packed));

struct madt_iso {
	struct madt_header hdr;
	uint8_t bus;
	uint8_t src;
	uint32_t gsi;
	uint16_t flags;
} __attribute__((packed));

struct madt_nmi_src {
	struct madt_header hdr;
	uint16_t flags;
	uint32_t gsi;
} __attribute__((packed));

struct madt_lapic_nmi {
	struct madt_header hdr;
	uint8_t acpi_uid;
	uint16_t flags;
	uint8_t LINTn;
} __attribute__((packed));

struct madt_lapic_addr_override {
	struct madt_header hdr;
	uint16_t reserved;
	uint64_t addr;
} __attribute__((packed));

struct madt_lx2apic {
	struct madt_header hdr;
	uint16_t reserved;
	uint32_t id;
	uint32_t flags;
	uint32_t acpi_uid;
} __attribute__((packed));

struct madt_lx2apic_nmi {
	struct madt_header hdr;
	uint16_t flags;
	uint32_t acpi_uid;
	uint8_t LINTn;
	uint16_t reserved0;
	uint8_t reserved1;
} __attribute__((packed));

#elif defined(__aarch64__)

enum gicc_flags {
	GICC_ENABLED = 1,

	GICC_PERFINT_LEVEL = 2,
	GICC_PERFINT_EDGE = 3,

	VGIC_MAINTENANCE_INT_LEVEL = 4,
	VGIC_MAINTENANCE_INT_EDGE = 5
};

enum gic_msi_frame_flags {
	SPI_IGNORE = 0,
	SPI_OVERRIDE = 1
};

struct madt_gicc {
	struct madt_header hdr;
	uint16_t reserved0;
	uint32_t interface_num;
	uint32_t acpi_uid;
	uint32_t flags;
	uint32_t parkprot_ver;
	uint32_t perf_int_gsiv;
	uint64_t parked_addr;
	uint64_t base_addr;
	uint64_t gicv;
	uint64_t gich;
	uint64_t vgic_maintenance_int;
	uint64_t gicr_base_addr;
	uint64_t mpidr;
	uint8_t powef_class;
	uint16_t reserved1;
	uint8_t reserved2;
} __attribute__((packed));

struct madt_gicd {
	struct madt_header hdr;
	uint16_t reserved0;
	uint32_t gic_id;
	uint64_t base_addr;
	uint32_t sysvec_base;
	uint8_t gic_ver;
	uint16_t reserved1;
	uint8_t reserved2;
} __attribute__((packed));

struct madt_gic_msi_frame {
	struct madt_header hdr;
	uint16_t reserved;
	uint32_t id;
	uint64_t base_addr;
	uint32_t flags;
	uint16_t spi_count;
	uint16_t spi_base;
} __attribute__((packed));

struct madt_gicr {
	struct madt_header hdr;
	uint16_t reserved;
	uint64_t discovery_range_base_addr;
	uint32_t discovery_range_len;
} __attribute__((packed));

struct madt_gic_its {
	struct madt_header hdr;
	uint16_t reserved0;
	uint32_t id;
	uint64_t base_addr;
	uint32_t reserved1;
} __attribute__((packed));

#endif

struct madt {
	struct sdt_header hdr;
	uint32_t lapic_addr;
	uint32_t flags;
	uint8_t structures[];
} __attribute__((packed));

void acpi_madt_init();

#endif /* _ACPI_MADT_H */