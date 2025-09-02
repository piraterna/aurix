/*********************************************************************************/
/* Module Name:  string.h */
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

#ifndef _LIB_STRING_H
#define _LIB_STRING_H

#include <stddef.h>

size_t mbstowcs(wchar_t *dest, const char **src, size_t len);

size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strncat(char *dest, const char *src, size_t n);
char *strdup(const char *s);
char *strtok(char *str, const char *delim);
char *strchr(char *s, int c);
char *strrchr(char *s, int c);

void *memset(void *dest, int val, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
int memcmp(const void *a, const void *b, size_t len);

#endif /* _LIB_STRING_H */