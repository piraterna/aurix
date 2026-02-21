/*********************************************************************************/
/* Module Name:  hpet.h */
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

#ifndef _ACPI_HPET_H
#define _ACPI_HPET_H

#include <acpi/acpi.h>
#include <stdint.h>

struct hpet_sdt {
	struct sdt_header hdr;
	uint32_t etblock_id;
	struct acpi_address addr;
	uint8_t hpet_num;
	uint16_t min_tick; // in periodic mode
	uint8_t page_prot;
} __attribute__((packed));

struct hpet_timer {
	volatile uint64_t conf_caps;
	volatile uint64_t comp_val;
	volatile uint64_t fsb_int_route;
	volatile uint64_t reserved;
} __attribute__((packed));

struct hpet {
	volatile uint64_t general_caps;
	volatile uint64_t reserved0;
	volatile uint64_t general_conf;
	volatile uint64_t reserved1;
	volatile uint64_t general_int_stat;
	volatile uint64_t reserved2;
	volatile uint64_t reserved3[2][12];
	volatile uint64_t main_count_val;
	volatile uint64_t reserved4;
	struct hpet_timer timers[];
} __attribute__((packed));

void acpi_hpet_init();
uint64_t hpet_get_ns(void);
int hpet_is_initialized(void);
void hpet_msleep(uint64_t ms);
void hpet_usleep(uint64_t us);
void hpet_nsleep(uint64_t ns);

#endif /* _ACPI_HPET_H */
