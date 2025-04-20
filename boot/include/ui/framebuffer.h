#ifndef _UI_FRAMEBUFFER_H
#define _UI_FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

enum fb_format {
	FB_RGBA,
	FB_BGRA
};

struct fb_mode {
	uint32_t width;
	uint32_t height;
	uint8_t bpp;
	uint32_t pitch;
	int format;
};

bool get_framebuffer(uintptr_t *fb_addr, struct fb_mode **available_modes, int *total_modes, int *current_mode_index);

#endif /* _UI_FRAMEBUFFER_H */