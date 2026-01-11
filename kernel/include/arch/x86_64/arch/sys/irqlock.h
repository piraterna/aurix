/*********************************************************************************/
/* Module Name:  irqlock.h */
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
#ifndef _SYS_IRQLOCK_H
#define _SYS_IRQLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <arch/cpu/cpu.h>

typedef struct {
	uint8_t lock;
	uint8_t irq_state;
} irqlock_t;

static inline uint8_t save_if(void)
{
	uint64_t rflags;
	__asm__ volatile("pushfq\n\t"
					 "popq %0"
					 : "=r"(rflags)
					 :
					 : "memory");
	return (rflags & (1 << 9)) != 0;
}

static inline void restore_if(uint8_t state)
{
	if (state)
		__asm__ volatile("sti" ::: "memory");
	else
		__asm__ volatile("cli" ::: "memory");
}

static inline void irqlock_init(irqlock_t *lock)
{
	lock->lock = 0;
	lock->irq_state = 0;
}

static inline void irqlock_acquire(irqlock_t *lock)
{
	lock->irq_state = save_if();
	__asm__ volatile("cli");

	while (__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE)) {
		__asm__ volatile("pause" ::: "memory");
	}
}

static inline void irqlock_release(irqlock_t *lock)
{
	__atomic_clear(&lock->lock, __ATOMIC_RELEASE);
	restore_if(lock->irq_state);
}

static inline bool irqlock_try_acquire(irqlock_t *lock)
{
	lock->irq_state = save_if();
	__asm__ volatile("cli");
	if (!__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE))
		return true;

	restore_if(lock->irq_state);
	return false;
}

static inline bool irqlock_held(irqlock_t *lock)
{
	return __atomic_load_n(&lock->lock, __ATOMIC_RELAXED) != 0;
}

#endif /* _SYS_IRQLOCK_H */