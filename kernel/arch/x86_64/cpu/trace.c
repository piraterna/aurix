/*********************************************************************************/
/* Module Name:  trace.c */
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

#include <arch/cpu/cpu.h>
#include <cpu/trace.h>
#include <debug/print.h>
#include <stdint.h>

void stack_trace(uint16_t max_depth)
{
	struct stack_frame *frame;
	__asm__ volatile("movq %%rbp, %0" : "=r"(frame)::"memory");

	for (uint64_t f = 0; frame && f < max_depth; f++) {
		if (frame->rip <= 0xffffffff80000000)
			break;

		klog("[%llx] 0x%.16llx\n", frame, frame->rip);
		frame = frame->rbp;
	}
}