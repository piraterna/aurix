/*********************************************************************************/
/* Module Name:  init.c                                                          */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <config/config.h>
#include <loader/loader.h>
#include <proto/aurix.h>
#include <uart/uart.h>
#include <vfs/vfs.h>
#include <ui/ui.h>
#include <axboot.h>
#include <print.h>

void axboot_init()
{
	uart_init(115200);

	if (!vfs_init("\\")) {
		error("axboot_init(): Failed to mount boot drive! Halting...\n");
		// TODO: Halt
		while (1);
	}

	config_init();

#if defined(__aarch64__) && !defined(AXBOOT_UEFI)
	struct axboot_entry rpi_kernel = {
		.name = "AurixOS for Raspberry Pi",
		.image_path = "\\System\\axkrnl",
		.protocol = PROTO_AURIX
	};
	loader_load(&rpi_kernel);
	UNREACHABLE();
#endif

#ifdef AXBOOT_UEFI
#include <driver.h>
	load_drivers();
#endif

	// boot straight away
	if (config_get_timeout() < 1) {
		struct axboot_entry *entries = config_get_entries();
		loader_load(&(entries[config_get_default()]));
	}

	ui_init();

	error("axboot_init(): Returned from main menu, something went wrong. Halting!");
	UNREACHABLE();
}