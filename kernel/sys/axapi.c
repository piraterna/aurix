/*********************************************************************************/
/* Module Name:  axapi.c */
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

#include <aurix/axapi.h>
#include <sys/axapi.h>

#include <string.h>

extern const struct axapi_export __axapi_exports_start[];
extern const struct axapi_export __axapi_exports_end[];

#undef AXAPI_SYM
#define AXAPI_SYM(ret, name, args)                                        \
	__attribute__((                                                       \
		section(".axapi.exports"),                                        \
		used)) static const struct axapi_export __axapi_export_##name = { \
		#name, (uintptr_t) & name                                         \
	};
#include <aurix/axapi_defs.inc>
#undef AXAPI_SYM

uintptr_t axapi_resolve(const char *name)
{
	if (!name)
		return 0;

	for (const struct axapi_export *e = __axapi_exports_start;
		 e < __axapi_exports_end; e++) {
		if (e->name && strcmp(e->name, name) == 0)
			return e->addr;
	}

	return 0;
}
