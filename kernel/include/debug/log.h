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
#include <time/time.h>

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

#ifndef LOG_VERBOSITY_SERIAL
#define LOG_VERBOSITY_SERIAL LOG_LEVEL_ALL
#endif

#ifndef LOG_VERBOSITY_DISPLAY
#define LOG_VERBOSITY_DISPLAY LOG_LEVEL_ERROR
#endif

#if LOG_COLOR
#define LOG_STYLE_RESET "\033[0m"
#define LOG_STYLE_PREFIX "\033[38;2;90;95;105m"
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

typedef int (*log_sink_fn_t)(const char *fmt, ...);

typedef struct {
	log_sink_fn_t fn;
	uint8_t verbosity;
} log_sink_t;

#define LOG_SINK_SERIAL 0
#define LOG_SINK_DISPLAY 1
#define LOG_SINK_MAX 2

static log_sink_t g_log_sinks[LOG_SINK_MAX] = {
	{ serial_kprintf, LOG_VERBOSITY_SERIAL },
	{ flanterm_kprintf, LOG_VERBOSITY_DISPLAY }
};

#define _log_dispatch(tag_style, line_style, level_str, level_val, fmt, ...) \
	do {                                                                     \
		uint64_t __ms = get_ms();                                            \
		uint32_t __s = (uint32_t)(__ms / 1000ull);                           \
		uint32_t __msr = (uint32_t)(__ms % 1000ull);                         \
		for (int __i = 0; __i < LOG_SINK_MAX; __i++) {                       \
			if (level_val <= g_log_sinks[__i].verbosity) {                   \
				g_log_sinks[__i].fn(LOG_STYLE_PREFIX                         \
									"[%u.%03u] " LOG_STYLE_RESET tag_style   \
									" %s " LOG_STYLE_RESET                   \
									" " line_style fmt LOG_STYLE_RESET,      \
									__s, __msr, level_str, ##__VA_ARGS__);   \
			}                                                                \
		}                                                                    \
	} while (0)

#define critical(fmt, ...)                                      \
	_log_dispatch(LOG_TAG_CRITICAL, LOG_LINE_CRITICAL, "crit ", \
				  LOG_LEVEL_CRITICAL, fmt, ##__VA_ARGS__)
#define error(fmt, ...)                                                    \
	_log_dispatch(LOG_TAG_ERROR, LOG_LINE_ERROR, "error", LOG_LEVEL_ERROR, \
				  fmt, ##__VA_ARGS__)
#define warn(fmt, ...)                                                       \
	_log_dispatch(LOG_TAG_WARN, LOG_LINE_WARN, "warn ", LOG_LEVEL_WARN, fmt, \
				  ##__VA_ARGS__)
#define info(fmt, ...)                                                       \
	_log_dispatch(LOG_TAG_INFO, LOG_LINE_INFO, "info ", LOG_LEVEL_INFO, fmt, \
				  ##__VA_ARGS__)
#define debug(fmt, ...)                                                    \
	_log_dispatch(LOG_TAG_DEBUG, LOG_LINE_DEBUG, "debug", LOG_LEVEL_DEBUG, \
				  fmt, ##__VA_ARGS__)
#define trace(fmt, ...)                                                    \
	_log_dispatch(LOG_TAG_TRACE, LOG_LINE_TRACE, "trace", LOG_LEVEL_TRACE, \
				  fmt, ##__VA_ARGS__)
#define test(fmt, ...)                                                       \
	_log_dispatch(LOG_TAG_TEST, LOG_LINE_TEST, "test ", LOG_LEVEL_TEST, fmt, \
				  ##__VA_ARGS__)
#define success(fmt, ...)                                                     \
	_log_dispatch(LOG_TAG_SUCCESS, LOG_LINE_SUCCESS, "ok   ", LOG_LEVEL_INFO, \
				  fmt, ##__VA_ARGS__)

static inline void log_set_sink_verbosity(int sink, uint8_t level)
{
	if (sink < LOG_SINK_MAX) {
		g_log_sinks[sink].verbosity = level;
	}
}

static inline __attribute__((deprecated("klog is deprecated"))) void
klog(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	kprintf(fmt, args);
	va_end(args);
}

void log_init(void);

#endif /* _LOG_H */