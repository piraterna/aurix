/*********************************************************************************/
/* Module Name:  pmm.h */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#ifndef _MM_PMM_H
#define _MM_PMM_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 0x1000

extern uint64_t bitmap_pages;
extern uint64_t used_pages;
extern uint64_t usable_pages;

void pmm_init(void);
void pmm_reclaim_bootparms(void);

void *palloc(size_t pages);
void pfree(void *ptr, size_t pages);

void pmm_ref_inc(uintptr_t phys, size_t pages);
void pmm_ref_dec(uintptr_t phys, size_t pages);
uint32_t pmm_refcount(uintptr_t phys);

uint64_t pmm_free_pages(void);
uint64_t pmm_used_pages(void);
uint64_t pmm_usable_pages(void);

#endif /* _MM_PMM_H */
