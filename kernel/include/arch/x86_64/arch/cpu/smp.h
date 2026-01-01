/*********************************************************************************/
/* Module Name:  smp.h */
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

#ifndef _ARCH_CPU_SMP_H
#define _ARCH_CPU_SMP_H

#include <stdint.h>

#define TRAMP_BASE_ADDR 0x8000

#define TRAMP_DATA_OFF 0x0F00
#define TRAMP_PML4 (TRAMP_DATA_OFF + 0x00)
#define TRAMP_ENTRY (TRAMP_DATA_OFF + 0x08)
#define TRAMP_CPU (TRAMP_DATA_OFF + 0x10)
#define TRAMP_STACK (TRAMP_DATA_OFF + 0x18)

void smp_init(void);

extern void smp_tramp_start(void);
extern void smp_tramp_end(void);

#define TRAMP_SIZE ((uintptr_t)smp_tramp_end - (uintptr_t)smp_tramp_start)

void smp_cpu_startup(uint8_t);

#endif /* _ARCH_CPU_SMP_H */
