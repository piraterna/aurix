/*********************************************************************************/
/* Module Name:  string.c */
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
#include <mm/mman.h>

#include <stddef.h>
#include <stdint.h>

size_t mbstowcs(wchar_t *dest, const char **src, size_t len)
{
	char *lsrc = (char *)*src;
	size_t count = len;

	if (dest == NULL) {
		return 0;
	}

	while (count) {
		if ((*dest = *lsrc) == 0) {
			lsrc = NULL;
			break;
		}

		if (*dest >= 0x80) {
			return -1;
		}

		lsrc++;
		dest++;
		count--;
	}

	return len - count;
}

size_t strspn(const char *s, const char *accept)
{
	const char *p;
	size_t count = 0;

	while (s != NULL) {
		for (p = accept; *p; p++) {
			if (*s == *p) {
				count++;
				break;
			}
		}

		if (!*p) {
			break;
		}
		s++;
	}

	return count;
}

size_t strcspn(const char *s, const char *reject)
{
	const char *p;
	size_t count = 0;

	while (*s) {
		for (p = reject; *p; p++) {
			if (*s == *p) {
				return count;
			}
		}
		s++;
		count++;
	}

	return count;
}

size_t strlen(const char *str)
{
	size_t count = 0;

	if (str == NULL) {
		return 0;
	}

	do {
		count++;
	} while (str[count] != '\0');

	return count;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}

	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	while (n-- && *s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}

	return n ? (*(unsigned char *)s1 - *(unsigned char *)s2) : 0;
}

char *strcpy(char *dest, const char *src)
{
	if (dest == NULL || src == NULL) {
		return NULL;
	}

	char *pdest = dest;

	while (*src != '\0') {
		*dest = *src;
		dest++;
		src++;
	}

	*dest = '\0';
	return pdest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	if (dest == NULL || src == NULL) {
		return NULL;
	}

	char *pdest = dest;

	while (*src != '\0' || n--) {
		*pdest++ = *src++;
	}

	*pdest = '\0';
	return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
	if (dest == 0 || src == 0) {
		return NULL;
	}

	size_t i = strlen(dest);
	size_t a = 0;

	while (a < n && src[a] != '\0') {
		dest[i++] = src[a++];
	}
	dest[i] = '\0';
	return dest;
}

// TODO: Get rid of this function
char *strdup(const char *s)
{
	size_t len = strlen(s);
	char *new = (char *)mem_alloc(len + 1);

	if (new) {
		strcpy(new, s);
	}

	return new;
}

char *strtok(char *str, const char *delim)
{
	static char *last;
	char *end;

	if (str == NULL) {
		str = last;
	}
	if (str == NULL) {
		return NULL;
	}

	str += strspn(str, delim);
	if (*str == '\0') {
		return NULL;
	}

	end = str + strcspn(str, delim);
	if (*end) {
		*end++ = '\0';
	}

	last = end;
	return str;
}

char *strchr(char *s, int c)
{
	if (s == NULL) {
		return NULL;
	}

	while (*s != 0) {
		if (*s == c)
			return (char *)s;
		s++;
	}

	return NULL;
}

char *strrchr(char *s, int c)
{
	const char *last = NULL;

	if (s == NULL) {
		return NULL;
	}

	while (*s != 0) {
		if (*s == (char)c)
			last = s;
		s++;
	}
	return (char *)last;
}

void *memset(void *dest, int val, size_t len)
{
	unsigned char *ptr = dest;
	while (len-- > 0) {
		*ptr++ = (unsigned char)val;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t len)
{
	char *d = (char *)dest;
	const char *s = (const char *)src;

	while (len-- > 0) {
		*d++ = *s++;
	}

	return dest;
}

int memcmp(const void *a, const void *b, size_t len)
{
	unsigned char *ap = (unsigned char *)a;
	unsigned char *bp = (unsigned char *)b;
	int ret = 0;

	if (a == b) {
		return 0;
	}

	while (len > 0) {
		if (*ap != *bp) {
			ret = (*ap > *bp) ? 1 : -1;
			break;
		}

		len--;
		ap++;
		bp++;
	}

	return ret;
}