/*********************************************************************************/
/* Module Name:  math.h */
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

#ifndef _ARCH_LIB_MATH_H
#define _ARCH_LIB_MATH_H

#include <stdbool.h>

static double _sqrt(double n)
{
	double out;
	__asm__ volatile("fldl %[input];"
					 "fsqrt;"
					 "fstpl %[output];"
					 : [output] "=g"(out)
					 : [input] "g"(n)
					 :);
	return out;
}

static double _cos(double n)
{
	double out;
	__asm__ volatile("fldl %[input];"
					 "fcos;"
					 "fstpl %[output];"
					 : [output] "=g"(out)
					 : [input] "g"(n)
					 :);
	return out;
}

static double _acos(double n)
{
	double imd = _sqrt(1 - (n * n));
	double out;
	__asm__ volatile("fldl %[imd];"
					 "fldl %[input];"
					 "fpatan;"
					 "fstpl %[output];"
					 : [output] "=g"(out)
					 : [input] "g"(n), [imd] "g"(imd)
					 :);
	return out;
}

static int _ifloor(double d)
{
	if ((((double)((int)d)) == d) || (d >= 0))
		return (int)d;
	return (int)(d - 1);
}

static int _iceil(double d)
{
	if ((((double)((int)d)) == d) || (d < 0))
		return (int)d;
	return (int)(d + 1);
}

static double _pow(double a, int n)
{
	double prod = 1;
	bool neg = (n < 0);
	n = neg ? -n : n;
	for (int i = 0; i < n; i++)
		prod = prod * a;
	if (neg)
		prod = 1 / prod;
	return prod;
}

static double _fmod(double x, double y)
{
	return x - ((int)(x / y)) * y;
}

#endif /* _ARCH_LIB_MATH_H */
