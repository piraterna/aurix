/*********************************************************************************/
/* Module Name:  paging.h */
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

#ifndef _MM_PAGING_H
#define _MM_PAGING_H

#include <firmware/memmap.h>

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 0x1000

int paging_init(struct memory_map_info *memmap);

void paging_identity_map(uint64_t addr);
void paging_map(uint64_t phys, uint64_t virt);
void paging_unmap(uint64_t virt);

void *paging_allocate(size_t np);

#endif /* _MM_PAGING_H */