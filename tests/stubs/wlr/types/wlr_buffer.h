#pragma once
#include "wlr/interfaces/wlr_buffer.h"
enum wlr_buffer_data_ptr_access_flag {
	WLR_BUFFER_DATA_PTR_ACCESS_READ = 1 << 0,
	WLR_BUFFER_DATA_PTR_ACCESS_WRITE = 1 << 1,
};
void wlr_buffer_drop(struct wlr_buffer *b);
struct wlr_buffer *wlr_buffer_lock(struct wlr_buffer *b);
void wlr_buffer_unlock(struct wlr_buffer *b);
bool wlr_buffer_begin_data_ptr_access(struct wlr_buffer *b, uint32_t flags, void **data, uint32_t *format, size_t *stride);
void wlr_buffer_end_data_ptr_access(struct wlr_buffer *b);
