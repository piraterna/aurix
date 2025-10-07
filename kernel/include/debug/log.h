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
#define C_INFO "\033[92m"
#define C_WARN "\033[93m"
#define C_ERR "\033[91m"
#define C_DBG "\033[94m"
#define C_RESET "\033[0m"
#else
#define C_INFO ""
#define C_WARN ""
#define C_ERR ""
#define C_DBG ""
#define C_RESET ""
#endif

#define info(fmt, ...) \
	kprintf(C_INFO "info @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define warn(fmt, ...) \
	kprintf(C_WARN "warn @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define error(fmt, ...) \
	kprintf(C_ERR "error @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)
#define debug(fmt, ...) \
	kprintf(C_DBG "debug @ %s(): " C_RESET fmt, __func__, ##__VA_ARGS__)

static inline __attribute__((deprecated("klog is deprecated"))) void
klog(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	kprintf(fmt, args);
	va_end(args);
}

#endif /* _LOG_H */