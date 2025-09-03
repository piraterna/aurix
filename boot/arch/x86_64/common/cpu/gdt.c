/*********************************************************************************/
/* Module Name:  gdt.c */
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

#include <arch/cpu/gdt.h>
#include <arch/mm/paging.h>

#include <stdint.h>

void gdt_set_entry(struct gdt_descriptor *entry, uint32_t base, uint32_t limit,
				   uint8_t access, uint8_t flags)
{
	entry->limit_low = limit & 0xffff;
	entry->base_low = base & 0xffff;
	entry->base_mid = (base >> 16) & 0xff;
	entry->access = access;
	entry->limit_flags = ((limit >> 16) & 0xf) | (flags << 4);
	entry->base_high = (base >> 24) & 0xff;
}