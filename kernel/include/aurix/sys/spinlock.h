/*********************************************************************************/
/* Module Name:  spinlock.h */
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

#ifndef _SYS_SPINLOCK_H
#define _SYS_SPINLOCK_H

#include <arch/cpu/cpu.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint8_t lock;
} spinlock_t;

static inline void spinlock_init(spinlock_t *lock)
{
	__atomic_clear(&lock->lock, __ATOMIC_RELAXED);
}

static inline void spinlock_acquire(spinlock_t *lock)
{
	while (__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE)) {
		cpu_spinwait();
	}
}

static inline void spinlock_release(spinlock_t *lock)
{
	__atomic_clear(&lock->lock, __ATOMIC_RELEASE);
}

static inline bool spinlock_try_acquire(spinlock_t *lock)
{
	return !__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE);
}

static inline bool spinlock_held(spinlock_t *lock)
{
	return __atomic_load_n(&lock->lock, __ATOMIC_RELAXED) != 0;
}

#endif /* _SYS_SPINLOCK_H */