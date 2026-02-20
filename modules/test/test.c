/*********************************************************************************/
/* Module Name:  test.c */
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
#include <aurix/axapi.h>
#include <test.h>

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "Test Module",
	.desc = "Test module for AurixOS",
	.author = "Kevin Alavik",

	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

int mod_init()
{
	while (1) {
		kprintf("Hello from test module! cpu=%u\n", cpu_get_current_id());
		sched_yield();
	}
	return 0;
}

void mod_exit()
{
}
