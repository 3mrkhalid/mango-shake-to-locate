#pragma once
#include "wlr/types/wlr_buffer.h"
struct wlr_xcursor_manager;
struct wlr_cursor {
	double x, y;
	struct wlr_buffer *buffer; /* image set through wlr_cursor_set_buffer */
	char xcursor_name[64];	   /* image set through wlr_cursor_set_xcursor */
	int hot_x, hot_y;
	float buffer_scale;
	int set_xcursor_calls, set_buffer_calls, unset_calls;
};
void wlr_cursor_set_buffer(struct wlr_cursor *c, struct wlr_buffer *b, int hx, int hy, float scale);
void wlr_cursor_unset_image(struct wlr_cursor *c);
void wlr_cursor_set_xcursor(struct wlr_cursor *c, struct wlr_xcursor_manager *m, const char *name);
