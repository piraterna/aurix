/*********************************************************************************/
/* Module Name:  apic.h */
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

#ifndef _ARCH_APIC_APIC_H
#define _ARCH_APIC_APIC_H

#include <stdint.h>

#define APIC_BASE_MSR 0x1b

enum apic_regs {
	APIC_ID = 0x20,
	APIC_VER = 0x30,
	APIC_TPR = 0x80,
	APIC_APR = 0x90,
	APIC_PPR = 0xA0,
	APIC_EOI = 0xB0,
	APIC_RRD = 0xC0,
	APIC_LOCAL_DEST = 0xD0,
	APIC_DEST_FORMAT = 0xE0,
	APIC_SPURIOUS_IVR = 0xF0,

	APIC_ERROR_STATUS = 0x280
};

enum ioapic_regs { IOAPICID = 0, IOAPICVER = 1, IOAPICARB = 2 };

#define IOAPICREDTBLL(n) (0x10 + 2 * (n))
#define IOAPICREDTBLH(n) (0x10 + 2 * (n) + 1)

enum ioapic_delivery_mode {
	IOAPIC_FIXED = 0,
	IOAPIC_LOW_PRIOR = 1,
	IOAPIC_SMI = 2,
	IOAPIC_NMI = 3,
	IOAPIC_INIT = 4,
	IOAPIC_EXTINT = 5
};

enum ioapic_destination_mode {
	IOAPIC_PHYSICAL_DESTINATION = 0,
	IOAPIC_LOGICAL_DESTINATION = 1
};

enum ioapic_delivery_status { IOAPIC_RELAXED = 0, IOAPIC_WAIT_FOR_PROCESS = 1 };

enum ioapic_pin_polarity { IOAPIC_ACTIVE_HI = 0, IOAPIC_ACTIVE_LO = 1 };

enum ioapic_trigger_mode { IOAPIC_TRIGGER_EDGE = 0, IOAPIC_TRIGGER_LEVEL = 1 };

union ioapic_redirect_entry {
	struct {
		uint8_t vec;
		uint8_t delivery_mode : 3;
		uint8_t destination_mode : 1;
		uint8_t delivery_status : 1;
		uint8_t pin_polarity : 1;
		uint8_t remote_irr : 1;
		uint8_t trigger_mode : 1;
		uint8_t mask : 1;
		uint64_t reserved : 39;
		uint8_t dest;
	} __attribute__((packed));
	struct {
		uint32_t low;
		uint32_t high;
	} __attribute__((packed)) bytes;

} __attribute__((packed));

uint64_t apic_msr_read(uint64_t offset);
void apic_msr_write(uint64_t offset, uint64_t val);

void apic_send_eoi();

uint32_t ioapic_read(uintptr_t base, uint8_t regoff);
void ioapic_write(uintptr_t base, uint8_t regoff, uint32_t data);
uint32_t lapic_read(uint16_t reg);
void lapic_write(uint16_t reg, uint32_t val);

void apic_init();

void ioapic_write_red(uint32_t gsi, uint8_t vec, uint8_t delivery_mode,
					  uint8_t polarity, uint8_t trigger_mode, uint8_t lapic_id);

#endif /* _ARCH_APIC_APIC_H */