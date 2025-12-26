/*********************************************************************************/
/* Module Name:  rtc.h                                                           */
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

#ifndef _DRIVERS_RTC_H
#define _DRIVERS_RTC_H

#include <stdint.h>

typedef struct {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t day;
	uint8_t month;
	uint16_t year;
	uint8_t weekday;
} rtc_time_t;

typedef enum {
	RTC_OK = 0,
	RTC_ERR_INVALID = 1, /* Invalid time/date */
	RTC_ERR_HW = 2, /* Hardware failure */
	RTC_ERR_NOT_INIT = 3 /* RTC not initialized */
} rtc_error_t;

rtc_error_t rtc_init(void);
rtc_error_t rtc_get_time(rtc_time_t *time);
rtc_error_t rtc_set_time(const rtc_time_t *time);

#endif /* _DRIVERS_RTC_H */