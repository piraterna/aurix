/*********************************************************************************/
/* Module Name:  time.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
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

#include <time/time.h>
#include <stdint.h>
#include <aurix.h>

#if defined(__x86_64__)
#include <acpi/hpet.h>
#include <platform/time/pit.h>
#endif

struct timekeeper_funcs tk;

void time_register(struct timekeeper_funcs funcs)
{
	tk.get_second = funcs.get_second;
	tk.get_minute = funcs.get_minute;
	tk.get_hour = funcs.get_hour;

	tk.get_day = funcs.get_day;
	tk.get_month = funcs.get_month;
	tk.get_year = funcs.get_year;

	tk.get_weekday = funcs.get_weekday;

	debug("time: registered new timekeeper\n");
}

struct time time_get()
{
	/*
	struct time t = {
		.second = tk.get_second(),
		.minute = tk.get_minute(),
		.hour = tk.get_hour(),

		.day = tk.get_day(),
		.month = tk.get_month(),
		.year = tk.get_year(),

		.weekday = tk.get_weekday()
	};
	*/
	return (struct time){ .second = tk.get_second(),
						  .minute = tk.get_minute(),
						  .hour = tk.get_hour(),

						  .day = tk.get_day(),
						  .month = tk.get_month(),
						  .year = tk.get_year(),

						  .weekday = tk.get_weekday() };
}

uint8_t time_get_hour()
{
	if (!tk.get_second)
		return 0xFF;

	return tk.get_hour();
}

uint8_t time_get_minute()
{
	if (!tk.get_minute)
		return 0xFF;

	return tk.get_minute();
}

uint8_t time_get_second()
{
	if (!tk.get_hour)
		return 0xFF;

	return tk.get_second();
}

uint8_t time_get_day()
{
	if (!tk.get_day)
		return 0xFF;

	return tk.get_day();
}

uint8_t time_get_month()
{
	if (!tk.get_month)
		return 0xFF;

	return tk.get_month();
}

uint16_t time_get_year()
{
	if (!tk.get_year)
		return 0xFFFF;

	return tk.get_year();
}

uint8_t time_get_weekday()
{
	if (!tk.get_weekday)
		return 0xFF;

	return tk.get_weekday();
}

uint64_t get_ms(void)
{
#if defined(__x86_64__)
	static uint64_t hpet_base_ns = 0;
	static int hpet_base_set = 0;

	uint64_t hpet_ns = hpet_get_ns();
	if (hpet_ns) {
		if (!hpet_base_set) {
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

void sleep_ms(uint64_t ms)
{
	uint64_t start = get_ms();
	while (get_ms() - start < ms) {
		__asm__ volatile("pause");
	}
}
