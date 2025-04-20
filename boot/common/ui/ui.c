/*********************************************************************************/
/* Module Name:  gui.c                                                           */
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

#include <ui/framebuffer.h>
#include <print.h>
#include <stdint.h>

struct ui_config {
	uintptr_t fb_addr;
	struct fb_mode *fb_modes;
	int total_modes;
	int current_mode;
};

struct ui_config config;

void ui_init()
{
	struct fb_mode *available_fb_modes = NULL;
	int total_modes = 0;
	int current_mode = 0;

	if (!get_framebuffer(&config.fb_addr, &config.fb_modes, &config.total_modes, &config.current_mode)) {
		debug("Failed to acquire a framebuffer!\n");
		while (1);
	}

	debug("Dumping framebuffer information\n");
	debug("--------------------------------\n");
	debug("Address: 0x%llx\n", config.fb_addr);

	for (int i = 0; i < config.total_modes; i++) {
		debug("\nMode %u:%s\n", i, (i == config.current_mode) ? " (current)" : "");
		debug("Resolution: %ux%u\n", config.fb_modes[i].width, config.fb_modes[i].height);
		debug("Bits Per Pixel: %u\n", config.fb_modes[i].bpp);
		debug("Pitch: %u\n", config.fb_modes[i].pitch);
	}

	while(1);
}