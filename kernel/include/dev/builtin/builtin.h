/*********************************************************************************/
/* Module Name:  builtin.h */
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

#ifndef _DEV_BUILTIN_BUILTIN_H
#define _DEV_BUILTIN_BUILTIN_H

#include <stddef.h>

struct builtin_dev_entry {
	const char *name;
	void (*init)(void);
};

extern const struct builtin_dev_entry builtin_dev_list[];
extern const size_t builtin_dev_count;

void builtin_dev_init(const struct builtin_dev_entry *list, size_t count);

#endif // _DEV_BUILTIN_BUILTIN_H
