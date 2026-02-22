/*********************************************************************************/
/* Module Name:  test.c */
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

#include <aurix/axapi.h>
#include <sys/aurix/mod.h>
#include <test.h>

#include <stddef.h>
#include <stdint.h>

static size_t cstrlen(const char *s)
{
	size_t n = 0;
	if (!s)
		return 0;
	while (s[n] != '\0')
		n++;
	return n;
}

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "Test Module",
	.desc = "Test module for AurixOS",
	.author = "Kevin Alavik",

	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

int mod_init()
{
	static const char *possible_ports[] = {
		"/dev/raw/serial/com1", "/dev/raw/serial/com2", "/dev/raw/serial/com3",
		"/dev/raw/serial/com4", "/dev/raw/serial/com5", "/dev/raw/serial/com6",
		"/dev/raw/serial/com7", "/dev/raw/serial/com8",
	};

	const char *active_ports[8];
	size_t active_count = 0;

	const char *msg = "hello from uart test module\r\n";
	size_t msg_len = cstrlen(msg);

	if (!ax_driver_exists || !ax_driver_is_ready) {
		kprintf("test-module: missing AXAPI driver poll funcs\n");
		for (;;)
			sched_yield();
	}
	kprintf("test-module: waiting for driver serial16550\n");

	while (!ax_driver_exists("serial16550") ||
		   !ax_driver_is_ready("serial16550")) {
		sched_yield();
	}

	kprintf("test-module: serial16550 driver ready\n");

	if (devfs_exists) {
		for (size_t i = 0;
			 i < sizeof(possible_ports) / sizeof(possible_ports[0]); i++) {
			if (devfs_exists(possible_ports[i])) {
				active_ports[active_count++] = possible_ports[i];
				kprintf("test-module: detected %s\n", possible_ports[i]);
			}
		}
	}

	if (active_count == 0) {
		kprintf("test-module: no serial ports found\n");
		for (;;)
			sched_yield();
	}

	while (1) {
		if (devfs_write) {
			for (size_t i = 0; i < active_count; i++) {
				devfs_write(active_ports[i], msg, msg_len);
			}
		}

		sched_yield();
	}

	return 0;
}

void mod_exit()
{
}
