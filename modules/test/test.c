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
		"/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3",
		"/dev/ttyS4", "/dev/ttyS5", "/dev/ttyS6", "/dev/ttyS7",
	};
	const char *msg = "hello from uart test module\r\n";
	size_t msg_len = cstrlen(msg);

	if (!ax_driver_exists || !ax_driver_is_ready) {
		kprintf("test-module: missing AXAPI driver poll funcs\n");
		for (;;)
			sched_yield();
	}

	for (;;) {
		if (!ax_driver_exists("serial16550")) {
			kprintf(
				"test-module: waiting for driver serial16550 to register\n");
			while (!ax_driver_exists("serial16550"))
				sched_yield();
		}
		if (!ax_driver_is_ready("serial16550")) {
			kprintf("test-module: waiting for driver serial16550 to bind\n");
			while (!ax_driver_is_ready("serial16550"))
				sched_yield();
		}
		break;
	}

	while (1) {
		if (devfs_exists && devfs_write) {
			for (size_t i = 0;
				 i < (sizeof(possible_ports) / sizeof(possible_ports[0]));
				 i++) {
				if (!devfs_exists(possible_ports[i]))
					continue;
				devfs_write(possible_ports[i], msg, msg_len);
			}
		}

		sched_yield();
	}
	return 0;
}

void mod_exit()
{
}
