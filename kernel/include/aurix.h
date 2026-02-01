/*********************************************************************************/
/* Module Name:  aurix.h */
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

#ifndef _AURIX_H
#define _AURIX_H

#include <config.h>
#include <debug/log.h>
#include <stdint.h>
#include <mm/vmm.h>

extern uintptr_t hhdm_offset;
extern vctx_t *kvctx;

#define PHYS_TO_VIRT(addr) ((uintptr_t)(addr) + hhdm_offset)
#define VIRT_TO_PHYS(addr) ((uintptr_t)(addr) - hhdm_offset)

#ifndef UNREACHABLE
#define UNREACHABLE() __builtin_unreachable()
#endif

#endif /* _AURIX_H */