/*********************************************************************************/
/* Module Name:  log.h */
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

#ifndef _LOG_H
#define _LOG_H

#include <util/kprintf.h>
#include <stdarg.h>

#define LOG_COLORS 1
#if LOG_COLORS
#define C_INFO "\033[92m" // Green
#define C_WARN "\033[93m" // Yellow
#define C_ERR "\033[91m" // Red
#define C_DBG "\033[94m" // Blue
#define C_TEST "\033[96m" // Light Cyan
#define C_TRACE "\033[95m" // Magenta
#define C_CRIT "\033[31m" // Bright Red
#define C_SUCCESS "\033[32m" // Bright Green
#define C_RESET "\033[0m"
#else
#define C_INFO ""
#define C_WARN ""
#define C_ERR ""
#define C_DBG ""
#define C_TEST ""
#define C_TRACE ""
#define C_CRIT ""
#define C_SUCCESS ""
#define C_PERF ""
#define C_RESET ""
#endif

#define info(fmt, ...) \
	kprintf(C_INFO "info @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define warn(fmt, ...) \
	kprintf(C_WARN "warn @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define error(fmt, ...) \
	kprintf(C_ERR "error @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define test(fmt, ...) \
	kprintf(C_TEST "test @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define critical(fmt, ...) \
	kprintf(C_CRIT "critical @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define success(fmt, ...) \
	kprintf(C_SUCCESS "success @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)

/* TODO: Use args to set verbosity (verbosity of 0 == no log at all) */
#if DEBUG
#define debug(fmt, ...) \
	kprintf(C_DBG "debug @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#else
#define debug(fmt, ...) (void)0
#endif

#if DEBUG
#define trace(fmt, ...) \
	kprintf(C_TRACE "trace @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#else
#define trace(fmt, ...) (void)0
#endif

static inline __attribute__((deprecated("klog is deprecated"))) void
klog(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	kprintf(fmt, args);
	va_end(args);
}

#endif /* _LOG_H */