/*********************************************************************************/
/* Module Name:  bitmap.h */
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

#ifndef _LIB_BITMAP_H
#define _LIB_BITMAP_H

#include <stdint.h>

static inline void bitmap_set(uint8_t *bitmap, uint64_t bit)
{
	bitmap[bit / 8] |= 1 << (bit % 8);
}

static inline void bitmap_clear(uint8_t *bitmap, uint64_t bit)
{
	bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline uint8_t bitmap_get(uint8_t *bitmap, uint64_t bit)
{
	return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

#endif /* _LIB_BITMAP_H */