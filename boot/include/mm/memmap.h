/*********************************************************************************/
/* Module Name:  memmap.h                                                        */
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

#ifndef _MEM_MEMMAP_H
#define _MEM_MEMMAP_H

#include <stdint.h>

enum AxBootMemMapType {
	MemMapReserved = 0,
	MemMapFaulty = 1,

	MemMapACPIReclaimable = 2,
	MemMapACPIMappedIO = 3,
	MemMapACPIMappedIOPortSpace = 4,
	MemMapACPINVS = 5,

	MemMapFirmware = 6,

	// stuff we can consider usable after loading the kernel
	MemMapFreeOnLoad = 7,

	MemMapUsable = 10,
};

typedef struct _axboot_memmap {
	uintptr_t base;
	uintptr_t size;
	int type;
} axboot_memmap;

axboot_memmap *get_memmap();

#endif /* _MEM_MEMMAP_H */