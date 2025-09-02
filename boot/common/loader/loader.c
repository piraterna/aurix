/*********************************************************************************/
/* Module Name:  loader.c */
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

#include <axboot.h>
#include <config/config.h>
#include <lib/string.h>
#include <loader/loader.h>
#include <print.h>
#include <proto/aurix.h>

int proto_str_to_int(char *proto)
{
	if (strcmp(proto, "aurix") == 0) {
		return PROTO_AURIX;
	}

#ifdef AXBOOT_UEFI
	if (strcmp(proto, "efi-chainload") == 0) {
		return PROTO_CHAINLOAD;
	}
#endif

	return PROTO_UNSUPPORTED;
}

void loader_load(struct axboot_entry *entry)
{
	debug("loader_load(): Booting \"%s\"...\n", entry->name);

	switch (entry->protocol) {
	case PROTO_AURIX: {
		aurix_load(entry->image_path);
		break;
	}
	default: {
		error("loader_load(): Entry doesn't have a supported protocol!\n");
		break;
	}
	}

	UNREACHABLE();
}