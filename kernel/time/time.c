/*********************************************************************************/
/* Module Name:  time.c */
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

#include <time/time.h>
#include <stdint.h>
#include <aurix.h>

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
