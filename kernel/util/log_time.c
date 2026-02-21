/*********************************************************************************/
/* Module Name:  log_time.c */
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

#include <debug/log.h>

#include <stdint.h>

#if defined(__x86_64__)
#include <acpi/hpet.h>
#include <platform/time/pit.h>
#endif

uint64_t log_uptime_ms(void)
{
#if defined(__x86_64__)
	/* Prefer HPET (runs even with IRQs off), fall back to PIT ticks. */
	static uint64_t hpet_base_ns = 0;
	static int hpet_base_set = 0;

	uint64_t hpet_ns = hpet_get_ns();
	if (hpet_ns) {
		if (!hpet_base_set) {
			/* Align HPET base to current PIT-derived ms so time doesn't jump. */
			uint64_t cur_ms = 0;
			if (pit_is_initialized()) {
				uint16_t hz = pit_get_hz();
				if (hz)
					cur_ms = (pit_get_ticks() * 1000ull) / (uint64_t)hz;
			}
			uint64_t cur_ns = cur_ms * 1000000ull;
			hpet_base_ns = (hpet_ns > cur_ns) ? (hpet_ns - cur_ns) : hpet_ns;
			hpet_base_set = 1;
		}
		return (hpet_ns - hpet_base_ns) / 1000000ull;
	}

	if (pit_is_initialized()) {
		uint16_t hz = pit_get_hz();
		if (!hz)
			return 0;
		return (pit_get_ticks() * 1000ull) / (uint64_t)hz;
	}

	return 0;
#else
	return 0;
#endif
}
