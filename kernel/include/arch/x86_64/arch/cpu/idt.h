/*********************************************************************************/
/* Module Name:  idt.h                                                           */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#ifndef _ARCH_CPU_IDT_H
#define _ARCH_CPU_IDT_H

#include <stdint.h>

struct idt_descriptor {
	uint16_t base_low;
	uint16_t codeseg;
	uint16_t flags;
	uint16_t base_mid;
	uint64_t base_high;
	uint64_t reserved;
};

struct idtr {
	uint16_t limit;
	uint64_t base;
};

void idt_init(void);

#endif /* _ARCH_CPU_IDT_H */
