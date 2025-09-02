/*********************************************************************************/
/* Module Name:  list.h */
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

#ifndef _DATA_LIST_H
#define _DATA_LIST_H

#include <stdint.h>

typedef struct _ListNode {
	void *data;
	struct _ListNode *prev;
	struct _ListNode *next;
} ListNode;

typedef struct _List {
	uint32_t count;
	ListNode *root;
} List;

List *list_new();

int list_add(List *list, void *data);
void *list_remove_at(List *list, uint32_t idx);

ListNode *listnode_new(void *data);

void *list_get_at(List *list, uint32_t idx);

#endif /* _DATA_LIST_H */
