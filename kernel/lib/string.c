/*********************************************************************************/
/* Module Name:  string.h */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#include <lib/string.h>
#include <aurix.h>
#include <mm/heap.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
	uint8_t *restrict pdest = (uint8_t *restrict)dest;
	const uint8_t *restrict psrc = (const uint8_t *restrict)src;

	for (size_t i = 0; i < n; i++) {
		pdest[i] = psrc[i];
	}

	return dest;
}

void *memset(void *s, int c, size_t n)
{
	uint8_t *p = (uint8_t *)s;

	for (size_t i = 0; i < n; i++) {
		p[i] = (uint8_t)c;
	}

	return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
	uint8_t *pdest = (uint8_t *)dest;
	const uint8_t *psrc = (const uint8_t *)src;

	if (src > dest) {
		for (size_t i = 0; i < n; i++) {
			pdest[i] = psrc[i];
		}
	} else if (src < dest) {
		for (size_t i = n; i > 0; i--) {
			pdest[i - 1] = psrc[i - 1];
		}
	}

	return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	const uint8_t *p1 = (const uint8_t *)s1;
	const uint8_t *p2 = (const uint8_t *)s2;

	for (size_t i = 0; i < n; i++) {
		if (p1[i] != p2[i]) {
			return p1[i] < p2[i] ? -1 : 1;
		}
	}

	return 0;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	while (n > 0 && *s1 && *s2) {
		if (*s1 != *s2) {
			return (unsigned char)*s1 - (unsigned char)*s2;
		}
		s1++;
		s2++;
		n--;
	}
	if (n == 0) {
		return 0;
	}
	return (unsigned char)*s1 - (unsigned char)*s2;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return (unsigned char)*s1 - (unsigned char)*s2;
}

size_t strlen(const char *str)
{
	const char *s = str;
	while (*s)
		s++;
	return s - str;
}

char *strcpy(char *restrict dest, const char *restrict src)
{
	if (!dest || !src) {
		return NULL;
	}

	uint8_t *restrict pdest = (uint8_t *restrict)dest;
	const uint8_t *restrict psrc = (const uint8_t *restrict)src;

	while (*psrc != '\0') {
		*pdest++ = *psrc++;
	}

	*pdest = '\0';

	return dest;
}

char *strncpy(char *restrict dest, const char *restrict src, size_t n)
{
	if (!dest || !src || n == 0) {
		return NULL;
	}

	uint8_t *restrict pdest = (uint8_t *restrict)dest;
	const uint8_t *restrict psrc = (const uint8_t *restrict)src;

	while (*psrc != '\0' || n-- == 0) {
		*pdest++ = *psrc++;
	}

	*pdest = '\0';

	return dest;
}

char *strtok(char *restrict str, const char *restrict delim)
{
	static char *saved;
	char *token_start;
	const char *d;

	if (str == NULL) {
		str = saved;
		if (str == NULL) {
			return NULL;
		}
	}

	while (*str) {
		for (d = delim; *d; d++) {
			if (*str == *d) {
				break;
			}
		}
		if (*d == '\0') {
			break;
		}
		str++;
	}

	if (*str == '\0') {
		saved = NULL;
		return NULL;
	}

	token_start = str;

	while (*str) {
		for (d = delim; *d; d++) {
			if (*str == *d) {
				*str = '\0';
				saved = str + 1;
				return token_start;
			}
		}
		str++;
	}

	saved = NULL;
	return token_start;
}

char *strdup(const char *s)
{
	if (!s)
		return NULL;
	size_t len = strlen(s);
	char *out = kmalloc(len + 1);
	if (!out)
		return NULL;
	for (size_t i = 0; i <= len; i++) {
		out[i] = s[i];
	}
	return out;
}

char *strtok_r(char *str, const char *delim, char **saveptr)
{
	char *s;

	if (str)
		s = str;
	else if (*saveptr)
		s = *saveptr;
	else
		return NULL;

	s += strspn(s, delim);
	if (*s == '\0') {
		*saveptr = NULL;
		return NULL;
	}

	char *token_end = s;
	while (*token_end && !strchr(delim, *token_end))
		token_end++;

	if (*token_end) {
		*token_end = '\0';
		*saveptr = token_end + 1;
	} else {
		*saveptr = NULL;
	}

	return s;
}

size_t strspn(const char *s, const char *accept)
{
	size_t len = 0;
	for (; *s; s++) {
		const char *a;
		for (a = accept; *a; a++) {
			if (*s == *a)
				break;
		}
		if (!*a)
			break;
		len++;
	}
	return len;
}

char *strchr(const char *s, int c)
{
	while (*s) {
		if (*s == (char)c)
			return (char *)s;
		s++;
	}
	return NULL;
}

int streq(const char *a, const char *b)
{
	if (!a || !b)
		return 0;
	while (*a && *b) {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}