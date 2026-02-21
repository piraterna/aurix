/*********************************************************************************/
/* Module Name: log.h                                                            */
/* Project:     AurixOS                                                          */
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

#ifndef _LOG_H
#define _LOG_H

#include <util/kprintf.h>
#include <stdarg.h>
#include <stdint.h>

/* TODO: Use boot args */
#ifndef DEBUG
#define DEBUG 1
#endif

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
/* end todo */

#if LOG_COLOR
#define LOG_COLOR_INFO "\033[92m" /* Green */
#define LOG_COLOR_WARN "\033[93m" /* Yellow */
#define LOG_COLOR_ERROR "\033[91m" /* Red */
#define LOG_COLOR_DEBUG "\033[94m" /* Blue */
#define LOG_COLOR_TEST "\033[96m" /* Light Cyan */
#define LOG_COLOR_TRACE "\033[95m" /* Magenta */
#define LOG_COLOR_CRITICAL "\033[31m" /* Bright Red */
#define LOG_COLOR_SUCCESS "\033[32m" /* Bright Green */
#define LOG_COLOR_RESET "\033[0m" /* Reset */
#else
#define LOG_COLOR_INFO ""
#define LOG_COLOR_WARN ""
#define LOG_COLOR_ERROR ""
#define LOG_COLOR_DEBUG ""
#define LOG_COLOR_TEST ""
#define LOG_COLOR_TRACE ""
#define LOG_COLOR_CRITICAL ""
#define LOG_COLOR_SUCCESS ""
#define LOG_COLOR_RESET ""
#endif

uint64_t log_uptime_ms(void);

#define _log_callback(color, level, fmt, ...)                           \
	do {                                                                \
		uint64_t __ms = log_uptime_ms();                                \
		kprintf("[%u.%03u] %s(): " color "%s: " LOG_COLOR_RESET fmt,    \
				(uint32_t)(__ms / 1000ull), (uint32_t)(__ms % 1000ull), \
				__func__, level, ##__VA_ARGS__);                        \
	} while (0)

#define critical(fmt, ...)                                                 \
	do {                                                                   \
		if (LOG_LEVEL_CRITICAL <= LOG_VERBOSITY)                           \
			_log_callback(LOG_COLOR_CRITICAL, "crit", fmt, ##__VA_ARGS__); \
	} while (0)

#define error(fmt, ...)                                                  \
	do {                                                                 \
		if (LOG_LEVEL_ERROR <= LOG_VERBOSITY)                            \
			_log_callback(LOG_COLOR_ERROR, "error", fmt, ##__VA_ARGS__); \
	} while (0)

#define warn(fmt, ...)                                                 \
	do {                                                               \
		if (LOG_LEVEL_WARN <= LOG_VERBOSITY)                           \
			_log_callback(LOG_COLOR_WARN, "warn", fmt, ##__VA_ARGS__); \
	} while (0)

#define info(fmt, ...)                                                 \
	do {                                                               \
		if (LOG_LEVEL_INFO <= LOG_VERBOSITY)                           \
			_log_callback(LOG_COLOR_INFO, "info", fmt, ##__VA_ARGS__); \
	} while (0)

#if DEBUG
#define debug(fmt, ...)                                                  \
	do {                                                                 \
		if (LOG_LEVEL_DEBUG <= LOG_VERBOSITY)                            \
			_log_callback(LOG_COLOR_DEBUG, "debug", fmt, ##__VA_ARGS__); \
	} while (0)

#define trace(fmt, ...)                                                  \
	do {                                                                 \
		if (LOG_LEVEL_TRACE <= LOG_VERBOSITY)                            \
			_log_callback(LOG_COLOR_TRACE, "trace", fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define debug(fmt, ...)
#define trace(fmt, ...)
#endif

#define test(fmt, ...)                                                 \
	do {                                                               \
		if (LOG_LEVEL_TEST <= LOG_VERBOSITY)                           \
			_log_callback(LOG_COLOR_TEST, "test", fmt, ##__VA_ARGS__); \
	} while (0)

#define success(fmt, ...)                                               \
	do {                                                                \
		if (LOG_LEVEL_INFO <= LOG_VERBOSITY)                            \
			_log_callback(LOG_COLOR_SUCCESS, "ok", fmt, ##__VA_ARGS__); \
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
