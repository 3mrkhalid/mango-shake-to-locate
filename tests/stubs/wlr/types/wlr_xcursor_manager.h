#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "wlr/xcursor.h"
struct wlr_xcursor_manager { uint32_t size; float loaded[8]; int n_loaded; };
struct wlr_xcursor_manager *wlr_xcursor_manager_create(const char *theme, uint32_t size);
void wlr_xcursor_manager_destroy(struct wlr_xcursor_manager *m);
bool wlr_xcursor_manager_load(struct wlr_xcursor_manager *m, float scale);
struct wlr_xcursor *wlr_xcursor_manager_get_xcursor(struct wlr_xcursor_manager *m, const char *name, float scale);
