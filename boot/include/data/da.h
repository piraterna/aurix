/*********************************************************************************/
/* Module Name:  da.b                                                            */
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

#ifndef _DATA_DA_H
#define _DATA_DA_H

#include <mm/mman.h>
#include <print.h>
#include <stddef.h>
#include <efilib.h>

#define DA_INIT_CAPACITY 8192
#define DA_REALLOC(oldptr, newsz) mem_realloc(oldptr, newsz)

#define da_append(da, item) \
	do {\
		if ((da)->count >= (da)->capacity) { \
			size_t new_capacity = (da)->capacity + DA_INIT_CAPACITY; \
			(da)->items = DA_REALLOC((da)->items, new_capacity * sizeof((da)->items[0])); \
			(da)->capacity = new_capacity; \
		} \
		(da)->items[(da)->count++] = (item); \
	} while (0)

#endif /* _DATA_DA_H */
