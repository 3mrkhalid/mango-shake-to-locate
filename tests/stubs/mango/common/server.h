#pragma once
#include <stdbool.h>
#include "wlr/types/wlr_cursor.h"
#include "wlr/types/wlr_xcursor_manager.h"
#include "wayland-server-core.h"
struct Server {
	struct wl_event_loop *event_loop;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_manager;
	bool cursor_hidden;
	int cursor_mode;
};
extern struct Server server;
