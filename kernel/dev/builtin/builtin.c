/*********************************************************************************/
/* Module Name:  builtin.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <dev/builtin/builtin.h>
#include <dev/builtin/null.h>
#include <dev/builtin/stdio.h>

const struct builtin_dev_entry builtin_dev_list[] = {
	{ .name = "stdio", .init = stdio_init },
	{ .name = "null", .init = null_init },
};

const size_t builtin_dev_count =
	sizeof(builtin_dev_list) / sizeof(builtin_dev_list[0]);

void builtin_dev_init(const struct builtin_dev_entry *list, size_t count)
{
	if (!list || count == 0) {
		return;
	}

	for (size_t i = 0; i < count; i++) {
		if (list[i].init) {
			list[i].init();
		}
	}
}
