/*********************************************************************************/
/* Module Name:  rtc.c                                                           */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE     */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <arch/cpu/cpu.h>
#include <platform/time/time.h>
#include <lib/string.h>
#include <time/time.h>
#include <aurix.h>
#include <stdbool.h>

#define CMOS_INDEX_PORT 0x70
#define CMOS_DATA_PORT 0x71

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_WEEKDAY 0x06
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B
#define RTC_CENTURY 0x32

static bool rtc_initialized = false;

static uint8_t bcd_to_bin(uint8_t bcd)
{
	return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(uint8_t bin)
{
	return ((bin / 10) << 4) | (bin % 10);
}

#define UPDATE_RETRY_AMOUNT 5
static bool wait_for_update(void)
{
	int retries = UPDATE_RETRY_AMOUNT;
	while (retries--) {
		int timeout = 100000;
		while (timeout--) {
			outb(CMOS_INDEX_PORT, RTC_STATUS_A);
			if (!(inb(CMOS_DATA_PORT) & 0x80)) {
				return true;
			}
		}
		io_wait();
	}
	error("RTC: Update timeout after %u retries\n", UPDATE_RETRY_AMOUNT);
	return false;
}

static inline uint8_t get_time(uint16_t time_port)
{
	if (!rtc_initialized) {
		error("RTC not initialized\n");
		return UINT8_MAX;
	}

	if (!wait_for_update()) {
		return UINT8_MAX;
	}

	outb(CMOS_INDEX_PORT, time_port);
	return bcd_to_bin(inb(CMOS_DATA_PORT));
}

static inline uint8_t get_hour(void)
{
	return get_time(RTC_HOURS);
}

static inline uint8_t get_minute(void)
{
	return get_time(RTC_MINUTES);
}

static inline uint8_t get_second(void)
{
	return get_time(RTC_SECONDS);
}

static inline uint8_t get_day(void)
{
	return get_time(RTC_DAY);
}

static inline uint8_t get_month(void)
{
	return get_time(RTC_MONTH);
}

static inline uint16_t get_year(void)
{
	return ((get_time(RTC_CENTURY) * 100) + get_time(RTC_YEAR));
}

static inline uint8_t get_weekday(void)
{
	return (uint8_t)get_time(RTC_WEEKDAY);
}

void platform_timekeeper_init(void)
{
	cpu_disable_interrupts();

	outb(CMOS_INDEX_PORT, RTC_STATUS_B);
	uint8_t status_b = inb(CMOS_DATA_PORT);
	status_b |= 0x02; /* 24-hour mode */
	status_b &= ~0x01; /* Disable daylight saving */

	/* Use BCD mode */
	outb(CMOS_INDEX_PORT, RTC_STATUS_B);
	outb(CMOS_DATA_PORT, status_b);
	io_wait();

	cpu_enable_interrupts();

	info("Initialized RTC (24-hour mode, no daylight saving)\n");
	rtc_initialized = true;

	struct timekeeper_funcs funcs = { .get_hour = get_hour,
									  .get_minute = get_minute,
									  .get_second = get_second,
									  .get_day = get_day,
									  .get_month = get_month,
									  .get_year = get_year,
									  .get_weekday = get_weekday };

	time_register(funcs);
}

void platform_set_time(const struct time time)
{
	if (!rtc_initialized) {
		error("RTC: Not initialized\n");
		return;
	}

	if (!wait_for_update()) {
		return;
	}

	outb(CMOS_INDEX_PORT, RTC_SECONDS);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.second));
	outb(CMOS_INDEX_PORT, RTC_MINUTES);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.minute));
	outb(CMOS_INDEX_PORT, RTC_HOURS);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.hour));
	outb(CMOS_INDEX_PORT, RTC_DAY);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.day));
	outb(CMOS_INDEX_PORT, RTC_MONTH);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.month));
	outb(CMOS_INDEX_PORT, RTC_YEAR);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.year % 100));
	outb(CMOS_INDEX_PORT, RTC_CENTURY);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.year / 100));
	outb(CMOS_INDEX_PORT, RTC_WEEKDAY);
	outb(CMOS_DATA_PORT, bin_to_bcd(time.weekday));
	io_wait();
}