#pragma once
#include <stdint.h>
#include "wlr/types/wlr_buffer.h"
struct wlr_xcursor_image {
	uint32_t width, height, hotspot_x, hotspot_y, delay;
	struct wlr_buffer *buf; /* stub-private */
};
struct wlr_xcursor {
	unsigned int image_count;
	struct wlr_xcursor_image **images;
	char *name;
	double total_delay;
};
struct wlr_buffer *wlr_xcursor_image_get_buffer(struct wlr_xcursor_image *image);
