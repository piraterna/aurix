/*********************************************************************************/
/* Module Name:  list.c */
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

#include <data/list.h>
#include <mm/mman.h>
#include <print.h>

#include <stdint.h>

List *list_new()
{
	List *list = (List *)mem_alloc(sizeof(List));
	if (!list) {
		debug("list_new(): Failed to allocate memory for new list!\n");
		return NULL;
	}

	list->count = 0;
	list->root = (ListNode *)0;

	return list;
}

ListNode *listnode_new(void *data)
{
	ListNode *ln = (ListNode *)mem_alloc(sizeof(ListNode));
	if (!ln) {
		debug("listnode_new(): Failed to allocate memory for new list node!\n");
		return NULL;
	}

	ln->prev = (ListNode *)0;
	ln->next = (ListNode *)0;
	ln->data = data;

	return ln;
}

int list_add(List *list, void *data)
{
	ListNode *node = listnode_new(data);
	if (!node) {
		return false;
	}

	if (!list->root) {
		list->root = node;
	} else {
		ListNode *cur = list->root;

		while (cur->next) {
			cur = cur->next;
		}

		cur->next = node;
		node->prev = cur;
	}

	list->count++;
	return true;
}

void *list_remove_at(List *list, uint32_t idx)
{
	void *data;

	if (!list || list->count == 0 || idx >= list->count) {
		return NULL;
	}

	ListNode *cur = list->root;

	for (uint32_t i = 0; (i < idx) && cur; i++) {
		cur = cur->next;
	}

	if (!cur) {
		return NULL;
	}

	data = cur->data;

	if (cur->prev) {
		cur->prev->next = cur->next;
	}

	if (cur->next) {
		cur->next->prev = cur->prev;
	}

	if (idx == 0) {
		list->root = cur->next;
	}

	mem_free(cur);

	list->count--;
	return data;
}

void *list_get_at(List *list, uint32_t idx)
{
	if (!list || list->count == 0 | idx >= list->count) {
		return NULL;
	}

	ListNode *cur = list->root;

	for (uint32_t i = 0; (i < idx) && cur; i++) {
		cur = cur->next;
	}

	return cur ? cur->data : NULL;
}