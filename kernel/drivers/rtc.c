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

#include <drivers/rtc.h>
#include <arch/util/io.h>
#include <debug/log.h>
#include <stdbool.h>
#include <lib/string.h>

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

static bool is_valid_time(const rtc_time_t *time)
{
	if (!time) {
		error("RTC: Null time pointer");
		return false;
	}
	if (time->seconds > 59) {
		warn("RTC: Invalid seconds: %d", time->seconds);
		return false;
	}
	if (time->minutes > 59) {
		warn("RTC: Invalid minutes: %d", time->minutes);
		return false;
	}
	if (time->hours > 23) {
		warn("RTC: Invalid hours: %d", time->hours);
		return false;
	}
	if (time->day < 1 || time->day > 31) {
		warn("RTC: Invalid day: %d", time->day);
		return false;
	}
	if (time->month < 1 || time->month > 12) {
		warn("RTC: Invalid month: %d", time->month);
		return false;
	}
	if (time->year < 1970) {
		warn("RTC: Invalid year: %d", time->year);
		return false;
	}
	if (time->weekday > 6) {
		warn("RTC: Invalid weekday: %d", time->weekday);
		return false;
	}
	return true;
}

static bool wait_for_update(void)
{
	int retries = 3;
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
	error("RTC: Update timeout after retries");
	return false;
}

rtc_error_t rtc_init(void)
{
	outb(CMOS_INDEX_PORT, RTC_STATUS_B);
	uint8_t status_b = inb(CMOS_DATA_PORT);
	status_b |= 0x02; /* 24-hour mode */
	status_b &= ~0x01; /* Disable daylight saving */
	/* Use BCD mode */
	outb(CMOS_INDEX_PORT, RTC_STATUS_B);
	outb(CMOS_DATA_PORT, status_b);
	io_wait();

	rtc_initialized = true;
	return RTC_OK;
}

rtc_error_t rtc_get_time(rtc_time_t *time)
{
	memset(time, 0, sizeof(rtc_time_t));
	if (!rtc_initialized) {
		error("RTC: Not initialized");
		return RTC_ERR_NOT_INIT;
	}
	if (!time) {
		error("RTC: Null time pointer");
		return RTC_ERR_INVALID;
	}

	if (!wait_for_update()) {
		return RTC_ERR_HW;
	}

	outb(CMOS_INDEX_PORT, RTC_SECONDS);
	time->seconds = inb(CMOS_DATA_PORT);
	outb(CMOS_INDEX_PORT, RTC_MINUTES);
	time->minutes = inb(CMOS_DATA_PORT);
	outb(CMOS_INDEX_PORT, RTC_HOURS);
	time->hours = inb(CMOS_DATA_PORT);
	outb(CMOS_INDEX_PORT, RTC_DAY);
	time->day = inb(CMOS_DATA_PORT);
	outb(CMOS_INDEX_PORT, RTC_MONTH);
	time->month = inb(CMOS_DATA_PORT);
	outb(CMOS_INDEX_PORT, RTC_YEAR);
	time->year = inb(CMOS_DATA_PORT);
	outb(CMOS_INDEX_PORT, RTC_WEEKDAY);
	time->weekday = inb(CMOS_DATA_PORT);

	time->seconds = bcd_to_bin(time->seconds);
	time->minutes = bcd_to_bin(time->minutes);
	time->hours = bcd_to_bin(time->hours);
	time->day = bcd_to_bin(time->day);
	time->month = bcd_to_bin(time->month);
	time->weekday = bcd_to_bin(time->weekday);

	uint8_t year_low = bcd_to_bin(time->year);
	uint16_t century = 20;
	outb(CMOS_INDEX_PORT, RTC_CENTURY);
	uint8_t century_bcd = inb(CMOS_DATA_PORT);
	if (century_bcd != 0xFF && century_bcd != 0x00) {
		century = bcd_to_bin(century_bcd);
	} else {
		error("RTC: Invalid century byte: 0x%02x", century_bcd);
	}
	time->year = century * 100 + year_low;

	if (!is_valid_time(time)) {
		return RTC_ERR_INVALID;
	}

	return RTC_OK;
}

rtc_error_t rtc_set_time(const rtc_time_t *time)
{
	if (!rtc_initialized) {
		error("RTC: Not initialized");
		return RTC_ERR_NOT_INIT;
	}
	if (!is_valid_time(time)) {
		return RTC_ERR_INVALID;
	}

	if (!wait_for_update()) {
		return RTC_ERR_HW;
	}

	uint8_t seconds = bin_to_bcd(time->seconds);
	uint8_t minutes = bin_to_bcd(time->minutes);
	uint8_t hours = bin_to_bcd(time->hours);
	uint8_t day = bin_to_bcd(time->day);
	uint8_t month = bin_to_bcd(time->month);
	uint8_t weekday = bin_to_bcd(time->weekday);
	uint8_t year = bin_to_bcd(time->year % 100);
	uint8_t century = bin_to_bcd(time->year / 100);

	outb(CMOS_INDEX_PORT, RTC_SECONDS);
	outb(CMOS_DATA_PORT, seconds);
	outb(CMOS_INDEX_PORT, RTC_MINUTES);
	outb(CMOS_DATA_PORT, minutes);
	outb(CMOS_INDEX_PORT, RTC_HOURS);
	outb(CMOS_DATA_PORT, hours);
	outb(CMOS_INDEX_PORT, RTC_DAY);
	outb(CMOS_DATA_PORT, day);
	outb(CMOS_INDEX_PORT, RTC_MONTH);
	outb(CMOS_DATA_PORT, month);
	outb(CMOS_INDEX_PORT, RTC_YEAR);
	outb(CMOS_DATA_PORT, year);
	outb(CMOS_INDEX_PORT, RTC_WEEKDAY);
	outb(CMOS_DATA_PORT, weekday);
	outb(CMOS_INDEX_PORT, RTC_CENTURY);
	outb(CMOS_DATA_PORT, century);
	io_wait();

	return RTC_OK;
}