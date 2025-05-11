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
		debug("axboot_init(): Failed to mount boot drive! Halting...\n");
		// TODO: Halt
		while (1);
	}

	//config_init();

	//ui_init();

	//debug("axboot_init(): Returned from main menu, something went wrong. Halting!");
	//UNREACHABLE();

	// just boot aurixos for now
	struct axboot_entry axos = {
		.name = "AurixOS",
		.description = "",
		.image_path = "\\System\\axkrnl",
		.protocol = PROTO_AURIX
	};
	loader_load(&axos);
	UNREACHABLE();
}