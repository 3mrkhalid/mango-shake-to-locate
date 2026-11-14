/* Minimal re-implementation of the wlroots/wayland calls used by the module.
 * It models the semantics the module relies on (buffer locks, xcursor name
 * de-duplication, scale lookup). It is NOT wlroots. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wlr/types/wlr_cursor.h"
#include "wlr/types/wlr_xcursor_manager.h"
#include "wayland-server-core.h"
#include "mango/common/server.h"
#include "mango/config/parse.h"
#include "mango/manage/monitor.h"

struct Server server;
struct Config config;

int g_inits, g_destroys;
int g_pessimistic_name_cache; /* 1: set_buffer does not forget xcursor name */
float g_monitor_scale = 1.0f;
struct wlr_output g_output = {1.0f};
Monitor g_monitor = {&g_output};

/* ---- timers */
struct wl_event_source *g_timer;
struct wl_event_source *wl_event_loop_add_timer(struct wl_event_loop *l, int (*cb)(void *), void *d) {
	(void)l;
	struct wl_event_source *s = calloc(1, sizeof(*s));
	s->cb = cb; s->data = d; g_timer = s;
	return s;
}
int wl_event_source_timer_update(struct wl_event_source *s, int ms) { s->armed_ms = ms; return 0; }
int wl_event_source_remove(struct wl_event_source *s) { if (g_timer == s) g_timer = NULL; free(s); return 0; }

/* ---- buffers */
void wlr_buffer_init(struct wlr_buffer *b, const struct wlr_buffer_impl *impl, int w, int h) {
	memset(b, 0, sizeof(*b)); b->impl = impl; b->width = w; b->height = h; g_inits++;
}
static void maybe_destroy(struct wlr_buffer *b) {
	if (b->dropped && b->n_locks == 0) { g_destroys++; b->impl->destroy(b); }
}
void wlr_buffer_drop(struct wlr_buffer *b) { b->dropped = true; maybe_destroy(b); }
struct wlr_buffer *wlr_buffer_lock(struct wlr_buffer *b) { b->n_locks++; return b; }
void wlr_buffer_unlock(struct wlr_buffer *b) { b->n_locks--; maybe_destroy(b); }
bool wlr_buffer_begin_data_ptr_access(struct wlr_buffer *b, uint32_t f, void **d, uint32_t *fmt, size_t *s) {
	if (b->accessing) { fprintf(stderr, "nested data ptr access\n"); abort(); }
	if (!b->impl->begin_data_ptr_access(b, f, d, fmt, s)) return false;
	b->accessing = true; return true;
}
void wlr_buffer_end_data_ptr_access(struct wlr_buffer *b) {
	if (!b->accessing) { fprintf(stderr, "end without begin\n"); abort(); }
	b->accessing = false; b->impl->end_data_ptr_access(b);
}

/* ---- cursor */
static void cursor_release(struct wlr_cursor *c) {
	if (c->buffer) { struct wlr_buffer *old = c->buffer; c->buffer = NULL; wlr_buffer_unlock(old); }
}
void wlr_cursor_set_buffer(struct wlr_cursor *c, struct wlr_buffer *b, int hx, int hy, float scale) {
	c->set_buffer_calls++;
	struct wlr_buffer *old = c->buffer;
	c->buffer = wlr_buffer_lock(b);
	if (old) wlr_buffer_unlock(old);
	if (!g_pessimistic_name_cache) c->xcursor_name[0] = 0;
	c->hot_x = hx; c->hot_y = hy; c->buffer_scale = scale;
}
void wlr_cursor_unset_image(struct wlr_cursor *c) {
	c->unset_calls++; cursor_release(c); c->xcursor_name[0] = 0;
}
void wlr_cursor_set_xcursor(struct wlr_cursor *c, struct wlr_xcursor_manager *m, const char *name) {
	(void)m;
	if (c->xcursor_name[0] && strcmp(c->xcursor_name, name) == 0) return; /* dedup like wlroots */
	c->set_xcursor_calls++;
	cursor_release(c);
	snprintf(c->xcursor_name, sizeof(c->xcursor_name), "%s", name);
}

/* ---- xcursor manager: images are synthetic gradients */
struct stub_pixbuf { struct wlr_buffer base; uint32_t *px; int w, h; size_t stride; };
static void pb_destroy(struct wlr_buffer *b) { struct stub_pixbuf *p = (struct stub_pixbuf *)b; free(p->px); free(p); }
static bool pb_begin(struct wlr_buffer *b, uint32_t f, void **d, uint32_t *fmt, size_t *s) {
	struct stub_pixbuf *p = (struct stub_pixbuf *)b; (void)f;
	*d = p->px; *fmt = 0x34325241u; *s = p->stride; return true;
}
static void pb_end(struct wlr_buffer *b) { (void)b; }
static const struct wlr_buffer_impl pb_impl = {pb_destroy, pb_begin, pb_end};

#define MAXC 32
struct cached { char name[16]; float scale; struct wlr_xcursor xc; struct wlr_xcursor_image img, *list[1]; struct stub_pixbuf *pb; };
struct mgr_priv { struct wlr_xcursor_manager pub; struct cached *c[MAXC]; int n; };

struct wlr_xcursor_manager *wlr_xcursor_manager_create(const char *t, uint32_t size) {
	(void)t; struct mgr_priv *m = calloc(1, sizeof(*m)); m->pub.size = size; return &m->pub;
}
void wlr_xcursor_manager_destroy(struct wlr_xcursor_manager *pub) {
	struct mgr_priv *m = (struct mgr_priv *)pub;
	for (int i = 0; i < m->n; i++) { free(m->c[i]->pb->px); free(m->c[i]->pb); free(m->c[i]); }
	free(m);
}
bool wlr_xcursor_manager_load(struct wlr_xcursor_manager *m, float scale) {
	for (int i = 0; i < m->n_loaded; i++) if (m->loaded[i] == scale) return true;
	if (m->n_loaded < 8) m->loaded[m->n_loaded++] = scale;
	return true;
}
struct wlr_xcursor *wlr_xcursor_manager_get_xcursor(struct wlr_xcursor_manager *pub, const char *name, float scale) {
	struct mgr_priv *m = (struct mgr_priv *)pub;
	int ok = 0; for (int i = 0; i < pub->n_loaded; i++) if (pub->loaded[i] == scale) ok = 1;
	if (!ok) return NULL;
	if (strcmp(name, "left_ptr") && strcmp(name, "default") && strcmp(name, "grab") && strcmp(name, "text")) return NULL;
	for (int i = 0; i < m->n; i++) if (!strcmp(m->c[i]->name, name) && m->c[i]->scale == scale) return &m->c[i]->xc;
	struct cached *c = calloc(1, sizeof(*c)); snprintf(c->name, sizeof(c->name), "%s", name); c->scale = scale;
	int side = (int)(pub->size * scale); if (side < 1) side = 1;
	struct stub_pixbuf *p = calloc(1, sizeof(*p));
	p->w = p->h = side; p->stride = (size_t)side * 4 + 8; /* odd stride on purpose */
	p->px = calloc(1, p->stride * side);
	for (int y = 0; y < side; y++) for (int x = 0; x < side; x++) {
		uint32_t a = 255, v = (uint32_t)((x * 255) / side);
		((uint32_t *)((char *)p->px + y * p->stride))[x] = a << 24 | v << 16 | (uint32_t)((y * 255) / side) << 8 | 0x40;
	}
	wlr_buffer_init(&p->base, &pb_impl, side, side); g_inits--; /* not module-owned */
	c->pb = p; c->img.width = c->img.height = side; c->img.hotspot_x = side / 4; c->img.hotspot_y = side / 8;
	c->img.buf = &p->base; c->list[0] = &c->img; c->xc.image_count = 1; c->xc.images = c->list;
	if (m->n < MAXC) m->c[m->n++] = c;
	return &c->xc;
}
struct wlr_buffer *wlr_xcursor_image_get_buffer(struct wlr_xcursor_image *i) { return i->buf; }

Monitor *monitor_at_point(double x, double y) { (void)x; (void)y; g_output.scale = g_monitor_scale; return &g_monitor; }
