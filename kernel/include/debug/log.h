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

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <time/time.h>

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_CRITICAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_TRACE 6
#define LOG_LEVEL_TEST 7
#define LOG_LEVEL_ALL 8

#ifndef LOG_MAX_LEVEL
#define LOG_MAX_LEVEL LOG_LEVEL_ALL
#endif

#define LOG_SINK_SERIAL 0
#define LOG_SINK_DISPLAY 1
#define LOG_SINK_MAX 8

typedef struct log_sink log_sink_t;

typedef void (*log_sink_write_fn)(log_sink_t *sink, const char *buf,
								  size_t len);
typedef void (*log_sink_flush_fn)(log_sink_t *sink);

struct log_sink {
	log_sink_write_fn write;
	log_sink_flush_fn flush;
	uint8_t min_level;
	uint8_t flags;
	void *priv;
};

#define LOG_SINK_FLAG_COLOR (1u << 0)
#define LOG_SINK_FLAG_TIME (1u << 1)
#define LOG_SINK_FLAG_FUNC (1u << 2)
#define LOG_SINK_FLAG_LEVEL (1u << 3)

#define LOG_STYLE_RESET "\033[0m"
#define LOG_STYLE_PREFIX "\033[90m"

#define LOG_TAG_CRITICAL "\033[31m"
#define LOG_TAG_ERROR "\033[91m"
#define LOG_TAG_WARN "\033[93m"
#define LOG_TAG_INFO "\033[94m"
#define LOG_TAG_DEBUG "\033[95m"
#define LOG_TAG_TRACE "\033[90m"
#define LOG_TAG_TEST "\033[96m"
#define LOG_TAG_SUCCESS "\033[92m"

#define LOG_LINE_CRITICAL "\033[31m"
#define LOG_LINE_ERROR "\033[91m"
#define LOG_LINE_WARN "\033[93m"
#define LOG_LINE_INFO "\033[97m"
#define LOG_LINE_DEBUG "\033[95m"
#define LOG_LINE_TRACE "\033[90m"
#define LOG_LINE_TEST "\033[96m"
#define LOG_LINE_SUCCESS "\033[92m"

#if defined(__GNUC__) || defined(__clang__)
#define LOG_FUNC __PRETTY_FUNCTION__
#else
#define LOG_FUNC __func__
#endif

static const char *const log_level_tags[8] = {
	[LOG_LEVEL_CRITICAL] = "CRIT", [LOG_LEVEL_ERROR] = "ERROR",
	[LOG_LEVEL_WARN] = "WARN",	   [LOG_LEVEL_INFO] = "INFO",
	[LOG_LEVEL_DEBUG] = "DEBUG",   [LOG_LEVEL_TRACE] = "TRACE",
	[LOG_LEVEL_TEST] = "TEST",	   [LOG_LEVEL_NONE] = "NONE",
};

static const char *const log_level_colors_tag[8] = {
	[LOG_LEVEL_CRITICAL] = LOG_TAG_CRITICAL, [LOG_LEVEL_ERROR] = LOG_TAG_ERROR,
	[LOG_LEVEL_WARN] = LOG_TAG_WARN,		 [LOG_LEVEL_INFO] = LOG_TAG_INFO,
	[LOG_LEVEL_DEBUG] = LOG_TAG_DEBUG,		 [LOG_LEVEL_TRACE] = LOG_TAG_TRACE,
	[LOG_LEVEL_TEST] = LOG_TAG_TEST,		 [LOG_LEVEL_NONE] = "",
};

static const char *const log_level_colors_line[8] = {
	[LOG_LEVEL_CRITICAL] = LOG_LINE_CRITICAL,
	[LOG_LEVEL_ERROR] = LOG_LINE_ERROR,
	[LOG_LEVEL_WARN] = LOG_LINE_WARN,
	[LOG_LEVEL_INFO] = LOG_LINE_INFO,
	[LOG_LEVEL_DEBUG] = LOG_LINE_DEBUG,
	[LOG_LEVEL_TRACE] = LOG_LINE_TRACE,
	[LOG_LEVEL_TEST] = LOG_LINE_TEST,
	[LOG_LEVEL_NONE] = "",
};

struct log_record {
	uint64_t timestamp;
	uint8_t level;
	uint16_t cpu_id;
	char func[64];
	char msg[256];
};

void log_init(void);
void log_sink_register(int id, log_sink_t *sink);
void log_sink_unregister(int id);
void log_set_level(int sink_id, uint8_t level);
void log_set_flags(int sink_id, uint8_t flags);

int log_write(uint8_t level, const char *func, const char *fmt, ...);
int log_vwrite(uint8_t level, const char *func, const char *fmt, va_list args);

void log_early_init(void);
int kprintf(const char *fmt, ...);

#ifdef CONFIG_KCONSOLE
void log_register_kcon_sink(void);
#endif

static inline int log_enabled(uint8_t level)
{
	return level <= LOG_MAX_LEVEL;
}

static inline int log_sink_enabled(const log_sink_t *sink, uint8_t level)
{
	return sink && level <= sink->min_level;
}

#define _log_dispatch(level, fmt, ...)                      \
	do {                                                    \
		if (log_enabled(level)) {                           \
			log_write(level, LOG_FUNC, fmt, ##__VA_ARGS__); \
		}                                                   \
	} while (0)

#define critical(fmt, ...) _log_dispatch(LOG_LEVEL_CRITICAL, fmt, ##__VA_ARGS__)
#define error(fmt, ...) _log_dispatch(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define warn(fmt, ...) _log_dispatch(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define info(fmt, ...) _log_dispatch(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define debug(fmt, ...) _log_dispatch(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define trace(fmt, ...) _log_dispatch(LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define test(fmt, ...) _log_dispatch(LOG_LEVEL_TEST, fmt, ##__VA_ARGS__)
#define success(fmt, ...) _log_dispatch(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

static inline __attribute__((deprecated("klog is deprecated, use info()"))) void
klog(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(LOG_LEVEL_INFO, LOG_FUNC, fmt, args);
	va_end(args);
}

#endif /* _LOG_H */