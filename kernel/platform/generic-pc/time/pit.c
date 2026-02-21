/*********************************************************************************/
/* Module Name:  pit.c */
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

#include <arch/cpu/irq.h>
#include <arch/cpu/cpu.h>
#include <platform/time/pit.h>
#include <aurix.h>
#include <stdint.h>
#include <stdbool.h>

static bool is_initialized = false;
static volatile uint64_t pit_ticks = 0;
static uint16_t pit_hz = 0;

void tick(void *ctx)
{
	(void)ctx;
	pit_ticks++;
}

void pit_init(uint16_t freq)
{
	if (is_initialized) {
		info("PIT is already initialized\n");
		return;
	}

	pit_ticks = 0;
	pit_hz = freq;

	uint16_t div = PIT_CLOCK / freq;

	outb(PIT_COMMAND, 0x36); // mode 3, rw
	outb(PIT_COUNTER0, div & 0xFF);
	outb(PIT_COUNTER0, div >> 8);
	irq_install(0, tick, NULL);

	is_initialized = true;

	debug("PIT is now running at %uHz (divisor = %u).\n", PIT_CLOCK / div, div);
}

int pit_is_initialized(void)
{
	return is_initialized ? 1 : 0;
}

uint16_t pit_get_hz(void)
{
	return pit_hz;
}

uint64_t pit_get_ticks(void)
{
	return pit_ticks;
}
