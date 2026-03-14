/*********************************************************************************/
/* Module Name:  mod.h */
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

#ifndef __SYS_AURIX_MOD_H
#define __SYS_AURIX_MOD_H

#include <stdint.h>

struct axmod_info {
	char *name;
	char *desc;
	char *author;

	int (*mod_init)(void);
	void (*mod_exit)(void);
} __attribute__((packed));

#ifndef MOD_NAME
#define MOD_NAME unknown
#endif

#define __AX_STR(x) #x
#define __AX_XSTR(x) __AX_STR(x)
#define MOD_COLOR "\x1b[38;2;100;100;100m"
#define MOD_RESET "\x1b[0m"

#define mod_log(fmt, ...) \
	kprintf(MOD_COLOR __AX_XSTR(MOD_NAME) ": " fmt MOD_RESET, ##__VA_ARGS__)

#endif /* __SYS_AURIX_MOD_H */
