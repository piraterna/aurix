/*********************************************************************************/
/* Module Name:  print.c                                                         */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#include <nanoprintf.h>

#include <config/config.h>
#include <uart/uart.h>
#include <vfs/vfs.h>
#include <print.h>

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include <efi.h>
#include <efilib.h>
#include <mm/mman.h>
#include <lib/string.h>

int32_t _fltused = 0;
int32_t __eqdf2 = 0;
int32_t __ltdf2 = 0;

extern struct axboot_cfg cfg;

CHAR16 wstr[1024];

void log(const char *fmt, ...)
{
	va_list args;
	char buf[1024];
	size_t size = 0;

	va_start(args, fmt);
	size = npf_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	uart_sendstr(buf);

	if (cfg.bootlog_filename != NULL) {
		vfs_write(cfg.bootlog_filename, (char *)&buf, size);
	}

#ifdef AXBOOT_UEFI
	char *bufp = &buf;
	size_t n = mbstowcs((wchar_t *)&wstr, (const char **)&bufp, 1024);
	wstr[n] = L'\r';
	wstr[n+1] = L'\0';
	gSystemTable->ConOut->OutputString(gSystemTable->ConOut, wstr);
	gBootServices->Stall(100000);
#endif
}

void debug(const char *fmt, ...)
{
	va_list args;
	char buf[1024];

	va_start(args, fmt);
	npf_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

#ifdef AXBOOT_UEFI
	char *bufp = &buf;
	size_t n = mbstowcs((wchar_t *)&wstr, (const char **)&bufp, 1024);
	wstr[n] = L'\r';
	wstr[n+1] = L'\0';
	gSystemTable->ConOut->OutputString(gSystemTable->ConOut, wstr);
	gBootServices->Stall(100000);
#endif
	uart_sendstr(buf);
}

void snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	npf_vsnprintf(buf, size, fmt, args);
	va_end(args);
}

void vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	npf_vsnprintf(buf, size, fmt, args);
}
