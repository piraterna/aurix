/*********************************************************************************/
/* Module Name:  dummy.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
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

#include <aurix/axapi.h>
#include <sys/aurix/mod.h>
#include <dev/driver.h>
#include <config.h>
#include <pci.h>

#include <stddef.h>
#include <stdint.h>

int mod_init(void);
void mod_exit(void);

#define PCI_CONFIG_VENDOR_ID 0x00
#define PCI_CONFIG_DEVICE_ID 0x02
#define PCI_CONFIG_COMMAND 0x04
#define PCI_CONFIG_STATUS 0x06
#define PCI_CONFIG_REVISION_ID 0x08
#define PCI_CONFIG_PROG_INTF 0x09
#define PCI_CONFIG_SUBCLASS 0x0a
#define PCI_CONFIG_CLASS_CODE 0x0b
#define PCI_CONFIG_CACHELINE_SIZE 0x0c
#define PCI_CONFIG_LATENCY 0x0d
#define PCI_CONFIG_HEADER_TYPE 0x0e
#define PCI_CONFIG_BIST 0x0f

#define PCI_TYPE_GENERIC 0x00
#define PCI_TYPE_BRIDGE 0x01
#define PCI_TYPE_CARDBUS_BRIDGE 0x02
#define PCI_TYPE_MULTIFUNC 0x80

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "pci",
	.desc = "PCI driver",
	.author = "Jozef Nagy",

	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

static int probe(struct device *dev)
{
	(void)dev;
	return 1;
}

static struct driver pci_driver = {
	.name = "pci",
	.class_name = "pci",
	.probe = probe,
	.remove = 0,
};

struct pci_device {
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
};

struct pci_devinfo {
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t class;
	uint8_t subclass;
	uint8_t prog_intf;
};

struct pci_device devlist[CONFIG_PCI_MAX_DEVICES];

uint8_t pci_read8(uint32_t id, uint32_t reg)
{
	uint32_t addr = 0x80000000 | id | (reg & 0xfc);
	ax_outdw(PCI_CONFIG_ADDR, addr);
	return ax_inb(PCI_CONFIG_DATA + (reg & 0x03));
}

uint16_t pci_read16(uint32_t id, uint32_t reg)
{
	uint32_t addr = 0x80000000 | id | (reg & 0xfc);
	ax_outdw(PCI_CONFIG_ADDR, addr);
	return ax_inw(PCI_CONFIG_DATA + (reg & 0x02));
}

void pci_visit(uint8_t bus, uint8_t dev, uint8_t func)
{
	uint32_t id = (bus << 16) | (dev << 11) | (func << 8);
	struct pci_devinfo info;
	info.vendor_id = pci_read16(id, PCI_CONFIG_VENDOR_ID);
	if (info.vendor_id == 0xffff) {
		return;
	}

	info.device_id = pci_read16(id, PCI_CONFIG_DEVICE_ID);
	info.prog_intf = pci_read8(id, PCI_CONFIG_PROG_INTF);
	info.subclass = pci_read8(id, PCI_CONFIG_SUBCLASS);
	info.class = pci_read8(id, PCI_CONFIG_CLASS_CODE);

	mod_log("Found device %02x:%02x:%d 0x%04x/0x%04x\n", bus, dev, func, info.vendor_id, info.device_id);
}

int mod_init()
{
	mod_log("Finding PCI devices...\n");
	for (uint8_t bus = 0; bus < 256; bus++) {
		for (uint8_t dev = 0; dev = 32; dev++) {
			uint32_t pciid = (bus << 16) | (dev << 11);
			uint8_t header_type = pci_read8(pciid, PCI_CONFIG_HEADER_TYPE);
			uint8_t func_count = header_type & PCI_TYPE_MULTIFUNC ? 8 : 1;

			for (uint8_t func = 0; func < func_count; func++) {
				pci_visit(bus, dev, func);
			}
		}
	}

	for (;;);

	return 0;
}

void mod_exit()
{
}
