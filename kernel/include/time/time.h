/*********************************************************************************/
/* Module Name:  time.h */
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

#ifndef _TIME_TIME_H
#define _TIME_TIME_H

#include <stdint.h>

struct timekeeper_funcs {
	uint8_t (*get_second)();
	uint8_t (*get_minute)();
	uint8_t (*get_hour)();

	uint8_t (*get_day)();
	uint8_t (*get_month)();
	uint16_t (*get_year)();

	uint8_t (*get_weekday)();
};

struct time {
	uint8_t second;
	uint8_t minute;
	uint8_t hour;

	uint8_t day;
	uint8_t month;
	uint16_t year;

	uint8_t weekday;
};

void time_register(struct timekeeper_funcs funcs);

struct time time_get();
uint8_t time_get_hour(void);
uint8_t time_get_minute(void);
uint8_t time_get_second(void);
uint8_t time_get_day(void);
uint8_t time_get_month(void);
uint16_t time_get_year(void);
uint8_t time_get_weekday(void);

#endif /* _TIME_TIME_H */