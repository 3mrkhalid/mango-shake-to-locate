#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct wlr_buffer;
struct wlr_buffer_impl {
	void (*destroy)(struct wlr_buffer *buffer);
	bool (*begin_data_ptr_access)(struct wlr_buffer *buffer, uint32_t flags,
								  void **data, uint32_t *format, size_t *stride);
	void (*end_data_ptr_access)(struct wlr_buffer *buffer);
};
struct wlr_buffer {
	const struct wlr_buffer_impl *impl;
	int width, height;
	bool dropped;
	size_t n_locks;
	bool accessing;
};
void wlr_buffer_init(struct wlr_buffer *b, const struct wlr_buffer_impl *impl, int w, int h);
