#pragma once
#include <stddef.h>
#include <stdint.h>
#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - offsetof(__typeof__(*sample), member))
struct wl_event_source {
	int (*cb)(void *);
	void *data;
	int armed_ms; /* 0 = disarmed */
};
struct wl_event_loop { int dummy; };
struct wl_event_source *wl_event_loop_add_timer(struct wl_event_loop *loop,
												int (*cb)(void *), void *data);
int wl_event_source_timer_update(struct wl_event_source *s, int ms);
int wl_event_source_remove(struct wl_event_source *s);
