/*********************************************************************************/
/* Module Name: log.h                                                            */
/* Project:     AurixOS                                                          */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy                                            */
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

#ifndef _LOG_H
#define _LOG_H

#include <util/kprintf.h>
#include <stdarg.h>
#include <stdint.h>

/* TODO: Use boot args */
#ifndef LOG_COLOR
#define LOG_COLOR 1
#endif

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_CRITICAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_TRACE 6
#define LOG_LEVEL_TEST 7
#define LOG_LEVEL_ALL 8

#ifndef LOG_VERBOSITY
#define LOG_VERBOSITY LOG_LEVEL_ALL
#endif

#ifndef LOG_OUTPUT_SCREEN
#define LOG_OUTPUT_SCREEN 0
#endif

#ifndef LOG_ERROR_SCREEN
#define LOG_ERROR_SCREEN 1
#endif

#if LOG_OUTPUT_SCREEN == 1
#define LOG_PRINTF kprintf
#else
#define LOG_PRINTF serial_kprintf
#endif

#if LOG_ERROR_SCREEN == 1
#define LOG_PRINTF_ERROR kprintf
#else
#define LOG_PRINTF_ERROR LOG_PRINTF
#endif

#define LOG_PRINTF_CRITICAL kprintf

/* end todo */

#if LOG_COLOR

#define LOG_STYLE_RESET "\033[0m"
#define LOG_STYLE_PREFIX "\033[38;2;90;95;105m"

#define LOG_BG_R 13
#define LOG_BG_G 15
#define LOG_BG_B 18

#define LOG_TAG_INFO "\033[1;38;2;13;15;18;48;2;100;150;200m"
#define LOG_TAG_WARN "\033[1;38;2;13;15;18;48;2;200;160;60m"
#define LOG_TAG_ERROR "\033[1;38;2;13;15;18;48;2;190;70;70m"
#define LOG_TAG_DEBUG "\033[1;38;2;13;15;18;48;2;140;120;170m"
#define LOG_TAG_TEST "\033[1;38;2;13;15;18;48;2;90;160;170m"
#define LOG_TAG_TRACE "\033[1;38;2;13;15;18;48;2;110;130;160m"
#define LOG_TAG_CRITICAL "\033[1;38;2;13;15;18;48;2;220;50;50m"
#define LOG_TAG_SUCCESS "\033[1;38;2;13;15;18;48;2;100;180;100m"

#define LOG_LINE_INFO "\033[38;2;170;180;190m"
#define LOG_LINE_WARN "\033[38;2;210;170;80m"
#define LOG_LINE_ERROR "\033[38;2;210;100;100m"
#define LOG_LINE_DEBUG "\033[38;2;150;130;180m"
#define LOG_LINE_TEST "\033[38;2;100;170;180m"
#define LOG_LINE_TRACE "\033[38;2;130;150;180m"
#define LOG_LINE_CRITICAL "\033[1;38;2;255;120;120m"
#define LOG_LINE_SUCCESS "\033[38;2;140;210;140m"

#else
#define LOG_STYLE_RESET ""
#define LOG_STYLE_PREFIX ""
#define LOG_TAG_INFO ""
#define LOG_TAG_WARN ""
#define LOG_TAG_ERROR ""
#define LOG_TAG_DEBUG ""
#define LOG_TAG_TEST ""
#define LOG_TAG_TRACE ""
#define LOG_TAG_CRITICAL ""
#define LOG_TAG_SUCCESS ""
#define LOG_LINE_INFO ""
#define LOG_LINE_WARN ""
#define LOG_LINE_ERROR ""
#define LOG_LINE_DEBUG ""
#define LOG_LINE_TEST ""
#define LOG_LINE_TRACE ""
#define LOG_LINE_CRITICAL ""
#define LOG_LINE_SUCCESS ""
#endif

#include <time/time.h>

#define _log_callback(tag_style, line_style, level, fmt, ...)              \
	do {                                                                   \
		uint64_t __ms = get_ms();                                          \
		LOG_PRINTF(LOG_STYLE_PREFIX "[%u.%03u] " LOG_STYLE_RESET tag_style \
									" %s " LOG_STYLE_RESET                 \
									" " line_style fmt LOG_STYLE_RESET,    \
				   (uint32_t)(__ms / 1000ull), (uint32_t)(__ms % 1000ull), \
				   level, ##__VA_ARGS__);                                  \
	} while (0)

#define _log_callback_error(tag_style, line_style, level, fmt, ...)         \
	do {                                                                    \
		uint64_t __ms = get_ms();                                           \
		LOG_PRINTF_ERROR(LOG_STYLE_PREFIX                                   \
						 "[%u.%03u] " LOG_STYLE_RESET tag_style             \
						 " %s " LOG_STYLE_RESET                             \
						 " " line_style fmt LOG_STYLE_RESET,                \
						 (uint32_t)(__ms / 1000ull),                        \
						 (uint32_t)(__ms % 1000ull), level, ##__VA_ARGS__); \
	} while (0)

#define _log_callback_critical(tag_style, line_style, level, fmt, ...)         \
	do {                                                                       \
		uint64_t __ms = get_ms();                                              \
		LOG_PRINTF_CRITICAL(LOG_STYLE_PREFIX                                   \
							"[%u.%03u] " LOG_STYLE_RESET tag_style             \
							" %s " LOG_STYLE_RESET                             \
							" " line_style fmt LOG_STYLE_RESET,                \
							(uint32_t)(__ms / 1000ull),                        \
							(uint32_t)(__ms % 1000ull), level, ##__VA_ARGS__); \
	} while (0)

#define critical(fmt, ...)                                              \
	do {                                                                \
		if (LOG_LEVEL_CRITICAL <= LOG_VERBOSITY)                        \
			_log_callback_critical(LOG_TAG_CRITICAL, LOG_LINE_CRITICAL, \
								   "crit ", fmt, ##__VA_ARGS__);        \
	} while (0)

#define error(fmt, ...)                                                      \
	do {                                                                     \
		if (LOG_LEVEL_ERROR <= LOG_VERBOSITY)                                \
			_log_callback_error(LOG_TAG_ERROR, LOG_LINE_ERROR, "error", fmt, \
								##__VA_ARGS__);                              \
	} while (0)

#define warn(fmt, ...)                                               \
	do {                                                             \
		if (LOG_LEVEL_WARN <= LOG_VERBOSITY)                         \
			_log_callback(LOG_TAG_WARN, LOG_LINE_WARN, "warn ", fmt, \
						  ##__VA_ARGS__);                            \
	} while (0)

#define info(fmt, ...)                                               \
	do {                                                             \
		if (LOG_LEVEL_INFO <= LOG_VERBOSITY)                         \
			_log_callback(LOG_TAG_INFO, LOG_LINE_INFO, "info ", fmt, \
						  ##__VA_ARGS__);                            \
	} while (0)

#define debug(fmt, ...)                                                \
	do {                                                               \
		if (LOG_LEVEL_DEBUG <= LOG_VERBOSITY)                          \
			_log_callback(LOG_TAG_DEBUG, LOG_LINE_DEBUG, "debug", fmt, \
						  ##__VA_ARGS__);                              \
	} while (0)

#define trace(fmt, ...)                                                \
	do {                                                               \
		if (LOG_LEVEL_TRACE <= LOG_VERBOSITY)                          \
			_log_callback(LOG_TAG_TRACE, LOG_LINE_TRACE, "trace", fmt, \
						  ##__VA_ARGS__);                              \
	} while (0)

#define test(fmt, ...)                                              \
	do {                                                            \
		if (LOG_LEVEL_TEST <= LOG_VERBOSITY)                        \
			_log_callback(LOG_TAG_TEST, LOG_LINE_TEST, "test", fmt, \
						  ##__VA_ARGS__);                           \
	} while (0)

#define success(fmt, ...)                                                  \
	do {                                                                   \
		if (LOG_LEVEL_INFO <= LOG_VERBOSITY)                               \
			_log_callback(LOG_TAG_SUCCESS, LOG_LINE_SUCCESS, "ok   ", fmt, \
						  ##__VA_ARGS__);                                  \
	} while (0)

static inline __attribute__((
	deprecated("klog is deprecated, use specific log macros instead"))) void
klog(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	kprintf(fmt, args);
	va_end(args);
}

void log_init(void);

#endif /* _LOG_H */
