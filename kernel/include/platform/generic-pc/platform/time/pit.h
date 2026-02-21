/*********************************************************************************/
/* Module Name:  pit.h */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#ifndef _PLATFORM_TIME_PIT_H
#define _PLATFORM_TIME_PIT_H

#include <stdint.h>

#define PIT_CLOCK 1193182
#define PIT_COMMAND 0x43
#define PIT_COUNTER0 0x40

void pit_init(uint16_t freq);

int pit_is_initialized(void);
uint16_t pit_get_hz(void);
uint64_t pit_get_ticks(void);

#endif /* _PLATFORM_TIME_PIT_H */
