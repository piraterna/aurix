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

#ifndef DEBUG
#define DEBUG 1
#endif

#define LOG_COLOR_INFO "\033[92m" /* Green */
#define LOG_COLOR_WARN "\033[93m" /* Yellow */
#define LOG_COLOR_ERROR "\033[91m" /* Red */
#define LOG_COLOR_DEBUG "\033[94m" /* Blue */
#define LOG_COLOR_TEST "\033[96m" /* Light Cyan */
#define LOG_COLOR_TRACE "\033[95m" /* Magenta */
#define LOG_COLOR_CRITICAL "\033[31m" /* Bright Red */
#define LOG_COLOR_SUCCESS "\033[32m" /* Bright Green */
#define LOG_COLOR_RESET "\033[0m" /* Reset */

// TODO: Proper time since boot or whatever
#define _log_callback(color, level, fmt, ...) \
	kprintf("[0.000] " color "%s: " LOG_COLOR_RESET fmt, level, ##__VA_ARGS__)

#define info(fmt, ...) _log_callback(LOG_COLOR_INFO, "info", fmt, ##__VA_ARGS__)

#define warn(fmt, ...) _log_callback(LOG_COLOR_WARN, "warn", fmt, ##__VA_ARGS__)

#define error(fmt, ...) \
	_log_callback(LOG_COLOR_ERROR, "error", fmt, ##__VA_ARGS__)

#define test(fmt, ...) _log_callback(LOG_COLOR_TEST, "test", fmt, ##__VA_ARGS__)

#define critical(fmt, ...) \
	_log_callback(LOG_COLOR_CRITICAL, "crit", fmt, ##__VA_ARGS__)

#define success(fmt, ...) \
	_log_callback(LOG_COLOR_SUCCESS, "ok", fmt, ##__VA_ARGS__)

#if DEBUG
#define debug(fmt, ...) \
	_log_callback(LOG_COLOR_DEBUG, "debug", fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...) ((void)0)
#endif

#if DEBUG
#define trace(fmt, ...) \
	_log_callback(LOG_COLOR_TRACE, "trace", fmt, ##__VA_ARGS__)
#else
#define trace(fmt, ...) ((void)0)
#endif

static inline __attribute__((
	deprecated("klog is deprecated, use specific log macros instead"))) void
klog(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	kprintf(fmt, args);
	va_end(args);
}

#endif /* _LOG_H */