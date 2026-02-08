/*********************************************************************************/
/* Module Name:  dummy.c */
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

#include <sys/aurix/mod.h>
#include <dummy.h>

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "Dummy Module",
	.desc = "Dummy module for AurixOS",
	.author = "Jozef Nagy",

	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

int mod_init()
{
	while (1) {
		__asm__ volatile("hlt");
	}
	return 0;
}

void mod_exit()
{
}