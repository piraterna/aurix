/*********************************************************************************/
/* Module Name:  smbios.h */
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

#ifndef _SMBIOS_SMBIOS_H
#define _SMBIOS_SMBIOS_H

#include <stdint.h>

enum {
	SMBIOS_FW_INFO = 0,
	SMBIOS_SYS_INFO = 1,
	SMBIOS_BASEBOARD_INFO = 2,
	SMBIOS_ENCLOSURE_INFO = 3,
	SMBIOS_PROCESSOR_INFO = 4,
	SMBIOS_CACHE_INFO = 7,
	SMBIOS_SYSSLOTS_INFO = 9,
	SMBIOS_PHYSMEM_ARRAY = 16,
	SMBIOS_MEMDEV_INFO = 17,
	SMBIOS_MEMARRAY_MAPPED_ADDR = 19,
	SMBIOS_SYS_BOOT_INFO = 32,
};

struct smbios_hdr {
	uint8_t type;
	uint8_t len;
	uint16_t handle;
} __attribute__((packed));

typedef struct {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_ver;
	uint8_t clock_seq_hi_and_reserved;
	uint8_t clock_seq_low;
	uint64_t node : 48;
} __attribute__((packed)) smbios_uuid;

struct smbios_fw_info {
	struct smbios_hdr hdr;
	uint8_t vendor_str;
	uint8_t fw_ver;
	uint16_t bios_startseg;
	uint8_t fw_release_date;
	uint8_t fw_rom_size;
	uint64_t fw_characteristics;
	uint8_t fw_characteristics_ext[2];
	uint8_t fw_maj_rel;
	uint8_t fw_min_rel;
	uint8_t ec_fw_maj_rel;
	uint8_t ec_fw_min_rel;
	uint16_t ext_fw_rom_size;
} __attribute__((packed));

typedef enum {
	SMBIOS_WAKEUP_RESERVED = 0x00,
	SMBIOS_WAKEUP_OTHER = 0x01,
	SMBIOS_WAKEUP_UNKNOWN = 0x02,
	SMBIOS_WAKEUP_APM_TIMER = 0x03,
	SMBIOS_WAKEUP_MODEM_RING = 0x04,
	SMBIOS_WAKEUP_LAN_REMOTE = 0x05,
	SMBIOS_WAKEUP_POWER_SWITCH = 0x06,
	SMBIOS_WAKEUP_PCI_PME = 0x07,
	SMBIOS_WAKEUP_AC_POWER_RESTORE = 0x08,
} smbios_wakeup;

struct smbios_sys_info {
	struct smbios_hdr hdr;
	uint8_t manufacturer_str;
	uint8_t product_str;
	uint8_t ver;
	uint8_t serial_num;
	smbios_uuid uuid;
	uint8_t wakeup_type;
	uint8_t sku_number_str;
	uint8_t family_str;
} __attribute__((packed));

typedef enum {
	SMBIOS_BOARD_UNKNOWN = 0x01,
	SMBIOS_BOARD_OTHER = 0x02,
	SMBIOS_SERVER_BLADE = 0x03,
	SMBIOS_CONNECTIVITY_SWITCH = 0x04,
	SMBIOS_SYSTEM_MANAGEMENT_MODULE = 0x05,
	SMBIOS_PROCESSOR_MODULE = 0x06,
	SMBIOS_IO_MODULE = 0x07,
	SMBIOS_MEMORY_MODULE = 0x08,
	SMBIOS_DAUGHTER_BOARD = 0x09,
	SMBIOS_MOTHERBOARD = 0x0A,
	SMBIOS_PROCMEM_MODULE = 0x0B,
	SMBIOS_PROCIO_MODULE = 0x0C,
	SMBIOS_INTERCONNECT_BOARD = 0x0D
} smbios_board_type;

struct smbios_baseboard_info {
	struct smbios_hdr hdr;
	uint8_t manufacturer_str;
	uint8_t product_str;
	uint8_t ver_str;
	uint8_t serial_num;
	uint8_t asset_tag;
	uint8_t features;
	uint8_t chassis_loc;
	uint16_t chassis_handle;
	uint8_t board_type;
	uint8_t contained_obj_handle_cnt;
	uint16_t contained_obj_handles[];
} __attribute__((packed));

typedef enum {
	SMBIOS_CHASSIS_STATE_OTHER = 0x01,
	SMBIOS_CHASSIS_STATE_UNKNOWN = 0x02,
	SMBIOS_CHASSIS_STATE_SAFE = 0x03,
	SMBIOS_CHASSIS_STATE_WARNING = 0x04,
	SMBIOS_CHASSIS_STATE_CRITICAL = 0x05,
	SMBIOS_CHASSIS_STATE_NON_RECOVERABLE = 0x06
} smbios_chassis_state;

typedef enum {
	SMBIOS_CHASSIS_SECURITY_OTHER = 0x01,
	SMBIOS_CHASSIS_SECURITY_UNKNOWN = 0x02,
	SMBIOS_CHASSIS_SECURITY_NONE = 0x03,
	SMBIOS_CHASSIS_SECURITY_EXTINTERF_LOCKED_OUT = 0x04,
	SMBIOS_CHASSIS_SECURITY_EXTINTERF_ENABLED = 0x05
} smbios_security_status;

typedef enum {
	SMBIOS_ENCLOSURE_OTHER = 0x01,
	SMBIOS_ENCLOSURE_UNKNOWN = 0x02,
	SMBIOS_DESKTOP = 0x03,
	SMBIOS_LOW_PROFILE_DESKTOP = 0x04,
	SMBIOS_PIZZA_BOX = 0x05,
	SMBIOS_MINI_TOWER = 0x06,
	SMBIOS_TOWER = 0x07,
	SMBIOS_PORTABLE = 0x08,
	SMBIOS_LAPTOP = 0x09,
	SMBIOS_NOTEBOOK = 0x0A,
	SMBIOS_HAND_HELD = 0x0B,
	SMBIOS_DOCKING_STATION = 0x0C,
	SMBIOS_AIO = 0x0D,
	SMBIOS_SUB_NOTEBOOK = 0x0E,
	SMBIOS_SPACE_SAVING = 0x0F,
	SMBIOS_LUNCH_BOX = 0x10,
	SMBIOS_MAIN_SERVER_CHASSIS = 0x11,
	SMBIOS_EXPANSION_CHASSIS = 0x12,
	SMBIOS_SUB_CHASSIS = 0x13,
	SMBIOS_BUS_EXPANSION_CHASSIS = 0x14,
	SMBIOS_PERIPHERAL_CHASSIS = 0x15,
	SMBIOS_RAID_CHASSIS = 0x16,
	SMBIOS_RACK_MOUNT_CHASSIS = 0x17,
	SMBIOS_SEALED_CASE_PC = 0x18,
	SMBIOS_MULTISYS_CHASSIS = 0x19,
	SMBIOS_COMPACT_PCI = 0x1A,
	SMBIOS_ADVANCED_TCA = 0x1B,
	SMBIOS_BLADE = 0x1C,
	SMBIOS_BLADE_ENCLOSURE = 0x1D,
	SMBIOS_TABLET = 0x1E,
	SMBIOS_CONVERTIBLE = 0x1F,
	SMBIOS_DETACHABLE = 0x20,
	SMBIOS_IOT_GATEWAY = 0x21,
	SMBIOS_EMBEDDED_PC = 0x22,
	SMBIOS_MINI_PC = 0x23,
	SMBIOS_STICK_PC = 0x24
} smbios_enclosure_type;

struct smbios_enclosure_info {
	struct smbios_hdr hdr;
	uint8_t manufacturer_str;
	uint8_t type;
	uint8_t ver_str;
	uint8_t serial_number;
	uint8_t asset_tag_num;
	uint8_t bootup_state;
	uint8_t psu_state;
	uint8_t thermal_state;
	uint8_t security_status;
	uint32_t oem_defined;
	uint8_t height; // u units
	uint8_t power_cord_cnt;
	uint8_t contained_element_cnt;
	uint8_t contained_element_len;
	uint8_t contained_elements[];
} __attribute__((packed));

struct smbios_enclosure_info27 {
	uint8_t sku_num_str;
	uint8_t rack_type;
	uint8_t rack_height;
} __attribute__((packed));

typedef enum {
	SMBIOS_PROCESSOR_OTHER = 0x01,
	SMBIOS_PROCESSOR_UNKNOWN = 0x02,
	SMBIOS_PROCESSOR_CENTRAL = 0x03,
	SMBIOS_PROCESSOR_MATH = 0x04,
	SMBIOS_PROCESSOR_DSP = 0x05,
	SMBIOS_PROCESSOR_VIDEO = 0x06
} smbios_processor_type;

struct smbios_processor_info {
	struct smbios_hdr hdr;
	uint8_t socket_desig_str;
	uint8_t proc_type;
	uint8_t proc_family;
	uint8_t proc_manufacturer;
	uint64_t proc_id;
	uint8_t proc_ver;
	uint16_t ext_clk;
	uint16_t max_speed;
	uint16_t cur_speed;
	uint8_t status;
	uint8_t proc_upgrade;
	uint16_t l1_handle;
	uint16_t l2_handle;
	uint16_t l3_handle;
	uint8_t serial_num;
	uint8_t asset_tag;
	uint8_t part_num;
	uint8_t core_count;
	uint8_t core_enabled;
	uint8_t thread_count;
	uint16_t proc_characteristics;
	uint16_t proc_family2;
	uint16_t core_count2;
	uint16_t core_enabled2;
	uint16_t thread_count2;
	uint16_t thread_enabled;
	uint8_t socket_type;
} __attribute__((packed));

struct smbios2_ep {
	char anchor_str[4];
	uint8_t checksum;
	uint8_t length;
	uint8_t major_ver;
	uint8_t minor_ver;
	uint16_t smbios_struct_maxsize;
	uint8_t revision;
	char format_area[5];
	char inter_anchor_str[5];
	uint8_t inter_checksum;
	uint16_t struct_tbl_len;
	uint32_t struct_tbl_addr;
	uint16_t struct_cnt;
	uint8_t bcd_rev;
} __attribute__((packed));

struct smbios3_ep {
	char anchor_str[5];
	uint8_t checksum;
	uint8_t length;
	uint8_t major_ver;
	uint8_t minor_ver;
	uint8_t docrev;
	uint8_t revision;
	uint8_t reserved;
	uint32_t struct_tbl_maxsize;
	uint64_t struct_tbl_addr;
} __attribute__((packed));

void smbios_init(void *);

#endif /* _SMBIOS_SMBIOS_H */