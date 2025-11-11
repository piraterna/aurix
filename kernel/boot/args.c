/*********************************************************************************/
/* Module Name:  args.c */
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

#include <mm/heap.h>
#include <lib/string.h>
#include <aurix.h>

void parse_boot_args(char *args)
{
	if (args == NULL || strlen(args) == 0) {
		debug("No boot arguments present, returning...\n");
		return;
	}

	// TODO: we need strtok

	// NOTE: dbglevel:
	//       0x01: Prevent reboot on panic
	//       0x02: Show symbols(TODO) on panic
	//       0x04: Send logs to a serial port
	//       0x08: Drop into a debugger on NMI
	//		 0x10: Drop into a debugger on #DB
}