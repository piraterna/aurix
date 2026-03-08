/*********************************************************************************/
/* Module Name:  ps2.c */
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
#include <ps2.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int mod_init(void);
void mod_exit(void);

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "i8042_ps2",
	.desc = "i8042 PS/2 Controller Driver",
	.author = "Jozef Nagy",

	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

struct ps2_info {
	bool is_dual_channel;

	bool port_enabled[2];
	uint16_t port_ident[2];
};

struct ps2_info info = { 0 };

void ps2_write_ctl_config(uint8_t val)
{
	ax_outb(PS2_COMMAND, 0x60);
	ax_outb(PS2_DATA, val);
}

uint8_t ps2_read_ctl_config()
{
	ax_outb(PS2_COMMAND, 0x20);
	return ax_inb(PS2_DATA);
}

void ps2_test_and_enable_port(uint8_t port)
{
	if (port == 0 || port >= 2) {
		kprintf(
			"i8042_ps2: Invalid port number supplied to ps2_test_and_enable_port(): %u\n",
			port);
		return;
	}

	if (port == 1) {
		ax_outb(PS2_COMMAND, 0xAB);
	} else if (port == 2) {
		ax_outb(PS2_COMMAND, 0xA9);
	}

	uint8_t test = ax_inb(PS2_DATA);
	if (test != 0) {
		kprintf("i8042_ps2: Port #%u failed self-test: ", port);
		switch (test) {
		case 1:
			kprintf("clock line stuck low\n");
			break;
		case 2:
			kprintf("clock line stuck high\n");
			break;
		case 3:
			kprintf("data line stuck low\n");
			break;
		case 4:
			kprintf("data line stuck high\n");
			break;
		default:
			kprintf("unknown error\n");
			break;
		}

		return;
	}

	// test passed, enable port
	uint8_t ctl_config = ps2_read_ctl_config();

	if (port == 1) {
		ax_outb(PS2_COMMAND, 0xAE);
		ctl_config |= (1 << 0);
	} else if (port == 2) {
		ax_outb(PS2_COMMAND, 0xA8);
		ctl_config |= (1 << 1);
	}

	ps2_write_ctl_config(ctl_config);

	info.port_enabled[port - 1] = true;
	kprintf("i8042_ps2: Enabled PS/2 port #%u\n", port);
}

bool ps2_wait_until_inbuf_free()
{
	uint64_t start_time = get_ms();
	uint64_t timeout_ms = 1000;

	while (ax_inb(PS2_STATUS) & (1 << 1)) {
		if (get_ms() - start_time > timeout_ms) {
			return false;
		}
	}

	return true;
}

bool ps2_send(uint8_t port, uint8_t val)
{
	if (port == 0 || port > 2) {
		kprintf("i8042_ps2: Invalid port number supplied to ps2_send(): %u\n",
				port);
		return false;
	}

	if (port == 2 && !info.port_enabled[1]) {
		kprintf(
			"i8042_ps2: Port #2 not enabled/present, writing is impossible.\n");
		return false;
	}

	if (port == 2)
		ax_outb(PS2_COMMAND, 0xD4);

	if (!ps2_wait_until_inbuf_free()) {
		kprintf(
			"i8042_ps2: Input buffer didn't empty in time, not writing data. (timeout expired)\n");
		return false;
	}

	ax_outb(PS2_DATA, val);
	return true;
}

void ps2_identify_device(uint8_t port)
{
	if (port == 0 || port >= 2) {
		kprintf(
			"i8042_ps2: Invalid port number supplied to ps2_identify_device(): %u\n",
			port);
		return;
	}

	uint64_t start_time = get_ms();
	uint64_t timeout_ms = 1000;

	ps2_send(port, 0xF5);
	while (ax_inb(PS2_DATA != 0xFA)) {
		if (get_ms() - start_time > timeout_ms) {
			return;
		}
	}

	start_time = get_ms();

	ps2_send(port, 0xF2);
	while (ax_inb(PS2_DATA != 0xFA)) {
		if (get_ms() - start_time > timeout_ms) {
			return;
		}
	}

	ax_inb(PS2_DATA);

	uint8_t ident[2] = { 0 };
	ident[0] = ax_inb(PS2_DATA);
	if (ident[0] == 0xAB || ident[0] == 0xAC) {
		ident[1] = ax_inb(PS2_DATA);
	}

	info.port_ident[port - 1] = ident[0] << 8 | ident[1];

	// enable scanning
	ps2_send(port, 0xF4);

	kprintf("i8042_ps2: Device present on port #%u: %x\n", port,
			info.port_ident[port - 1]);
}

void ps2_reset_port(uint8_t port)
{
	if (port == 0 || port > 2) {
		kprintf(
			"i8042_ps2: Invalid port number supplied to ps2_reset_port(): %u\n",
			port);
		return;
	}

	if (info.port_enabled[port - 1]) {
		kprintf("i8042_ps2: Sending RESET byte to PS/2 device #%u...\n", port);
		if (!ps2_send(port, 0xFF)) {
			info.port_enabled[port - 1] = false;
		}

		uint8_t res[2];
		res[0] = ax_inb(PS2_DATA);
		res[1] = ax_inb(PS2_DATA);

		if ((res[0] != 0xFA && res[1] != 0xAA)) {
			kprintf(
				"i8042_ps2: Device on port #%u did not reset successfully\n",
				port);
			info.port_enabled[port - 1] = false;
			return;
		}
	}
}

void ps2_reset_all()
{
	ps2_reset_port(1);
	ps2_reset_port(2);
}

int mod_init()
{
	if (!driver_poll) {
		kprintf("i8042_ps2: Missing AXAPI driver poll func\n");
		for (;;)
			;
	}

	uint8_t ctl_config = 0;

	// TODO: Determine if a PS/2 controller exists
	kprintf(
		"i8042_ps2: PS/2 Controller check is not implemented yet, assuming there is one.\n");

	kprintf("i8042_ps2: Sending disable command to PS/2 controller...\n");
	ax_outb(PS2_COMMAND, 0xAD);
	ax_outb(PS2_COMMAND, 0xA7);

	// flush output buffer
	ax_inb(PS2_DATA);

	ctl_config = ps2_read_ctl_config();

	// disable IRQs and translation for port 1
	ctl_config &= (1 << 0);
	ctl_config &= (1 << 6);

	// enable clock signal
	ctl_config &= (1 << 4);

	ps2_write_ctl_config(ctl_config);

	// perform self test
	ax_outb(PS2_COMMAND, 0xAA);
	uint8_t selftest = ax_inb(PS2_DATA);
	if (selftest != 0x55) {
		kprintf("i8042_ps2: PS/2 Controller self-test failed (replied %x)!",
				selftest);
		for (;;)
			;
	}

	// incase the controller was reset during self test
	ps2_write_ctl_config(ctl_config);

	// enable second channel if there is one
	ax_outb(PS2_COMMAND, 0xA8);
	ctl_config = ps2_read_ctl_config();
	if (ctl_config & (1 << 5)) {
		info.is_dual_channel = true;

		ax_outb(PS2_COMMAND, 0xA7);
		ctl_config &= (1 << 0);
		ctl_config &= (1 << 5);
		ps2_write_ctl_config(ctl_config);
	}

	// test and enable both ports
	ps2_test_and_enable_port(1);
	if (info.is_dual_channel) {
		ps2_test_and_enable_port(2);
	}

	// are any ports okay?
	if (!info.port_enabled[0] && !info.port_enabled[1]) {
		kprintf("i8042_ps2: No ports were successfully enabled, exitting.\n");
		// TODO: Module unloading
		for (;;)
			;
	}

	ps2_reset_all();

	if (info.port_enabled[0]) {
		ps2_identify_device(1);
	}

	if (info.port_enabled[1]) {
		ps2_identify_device(2);
	}

	kprintf(
		"i8042_ps2: Successfully initialized i8042 PS/2 %s-Channel Controller\n",
		info.is_dual_channel ? "Dual" : "Single");

	// for now just hang
	for (;;)
		;

	return 0;
}

void mod_exit()
{
}
