#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mango/input/shake_to_locate.h"
#include "mango/input/pointer.h"
#include "mango/common/server.h"
#include "mango/config/parse.h"

extern int g_inits, g_destroys, g_pessimistic_name_cache;
extern float g_monitor_scale;
extern struct wl_event_source *g_timer;

static struct wl_event_loop loop;
static struct wlr_cursor cur;
static uint32_t T = 1000; /* synthetic device time (ms) */
static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("  FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void defaults(void) {
	config.cursor_theme = "test"; config.cursor_size = 24;
	config.shake_to_locate = 1; config.shake_to_locate_sensitivity = 1.0f;
	config.shake_to_locate_min_distance = 90; config.shake_to_locate_intensity = 1.0f;
	config.shake_to_locate_cooldown = 1200; config.shake_to_locate_max_size = 0;
}
static void fresh(void) {
	shake_to_locate_finish();
	if (cur.buffer) wlr_cursor_unset_image(&cur);
	memset(&cur, 0, sizeof(cur)); cur.x = 100; cur.y = 100;
	server.event_loop = &loop; server.cursor = &cur; server.cursor_hidden = false; server.cursor_mode = CurNormal;
	if (server.cursor_manager) wlr_xcursor_manager_destroy(server.cursor_manager);
	server.cursor_manager = wlr_xcursor_manager_create("test", 24);
	wlr_xcursor_manager_load(server.cursor_manager, 1.0f);
	wlr_xcursor_manager_load(server.cursor_manager, 2.0f);
	g_monitor_scale = 1.0f; g_pessimistic_name_cache = 0;
	defaults();
	shake_to_locate_init();
	shake_to_locate_reapply();
	shake_to_locate_set_cursor("left_ptr");
}
static void fire_timer(void) { if (g_timer && g_timer->armed_ms) g_timer->cb(g_timer->data); }
static bool timer_armed(void) { return g_timer && g_timer->armed_ms; }

/* feed an alternating motion burst; also runs the animation timer */
static bool shake_for(int ms, double amp, int *max_w, int *distinct) {
	bool trig = false; int last = -1;
	for (int t = 0; t < ms; t += 8) {
		T += 8;
		if (shake_to_locate_motion(T, ((t / 8) & 1) ? -amp : amp, 0)) trig = true;
		usleep(8000); fire_timer();
		if (cur.buffer) {
			if (max_w && cur.buffer->width > *max_w) *max_w = cur.buffer->width;
			if (distinct && cur.buffer->width != last) { (*distinct)++; last = cur.buffer->width; }
		}
	}
	return trig;
}
static void settle(int ms) { for (int t = 0; t < ms && timer_armed(); t += 8) { usleep(8000); fire_timer(); } }

int main(void) {
	int w, d;

	printf("1. slow straight movement never triggers\n");
	fresh();
	for (int i = 0; i < 200; i++) { T += 8; CHECK(!shake_to_locate_motion(T, 5, 0), "triggered"); }
	CHECK(!timer_armed() && !cur.buffer, "animation started");

	printf("2. shake triggers, cursor grows continuously (not stepwise)\n");
	fresh(); w = 0; d = 0;
	bool trig = shake_for(900, 20, &w, &d);
	CHECK(trig, "no trigger");
	CHECK(cur.buffer != NULL, "no enlarged buffer");
	CHECK(w > 24 * 8, "max width only %d", w);
	CHECK(d > 40, "only %d distinct sizes (stepwise?)", d);
	printf("   max width %d px, %d distinct sizes\n", w, d);

	printf("3. idle -> shrinks back, normal cursor restored, no leaks\n");
	settle(3000);
	CHECK(!timer_armed(), "timer still armed");
	CHECK(cur.buffer == NULL, "enlarged buffer still on cursor");
	CHECK(!strcmp(cur.xcursor_name, "left_ptr"), "name '%s'", cur.xcursor_name);
	CHECK(g_inits == g_destroys, "buffers created %d destroyed %d", g_inits, g_destroys);

	printf("4. pessimistic wlroots (set_buffer keeps xcursor name): restore still works\n");
	fresh(); g_pessimistic_name_cache = 1;
	shake_for(400, 20, NULL, NULL); settle(3000);
	CHECK(cur.buffer == NULL && !strcmp(cur.xcursor_name, "left_ptr"), "stuck enlarged (buf=%p)", (void *)cur.buffer);

	printf("5. cooldown honoured, then allowed again\n");
	fresh(); config.shake_to_locate_cooldown = 800;
	CHECK(shake_for(300, 20, NULL, NULL), "first shake");
	settle(3000);
	T += 100; /* well inside cooldown on the device clock */
	bool again = false; for (int i = 0; i < 30; i++) { T += 8; if (shake_to_locate_motion(T, (i & 1) ? -20 : 20, 0)) again = true; }
	CHECK(!again, "re-triggered during cooldown");
	T += 2000;
	again = false; for (int i = 0; i < 30; i++) { T += 8; if (shake_to_locate_motion(T, (i & 1) ? -20 : 20, 0)) again = true; }
	CHECK(again, "not re-triggered after cooldown");
	settle(3000);

	printf("6. disabled in config -> nothing happens\n");
	fresh(); config.shake_to_locate = 0;
	CHECK(!shake_for(300, 20, NULL, NULL) && !cur.buffer, "ran while disabled");

	printf("7. sensitivity / min distance\n");
	fresh(); config.shake_to_locate_min_distance = 2000;
	CHECK(!shake_for(200, 10, NULL, NULL), "triggered with huge min distance");
	fresh(); config.shake_to_locate_min_distance = 90; config.shake_to_locate_sensitivity = 0.1f; /* needs 900px */
	CHECK(!shake_for(100, 10, NULL, NULL), "low sensitivity triggered too early");
	fresh(); config.shake_to_locate_sensitivity = 10.0f; /* needs 9px */
	CHECK(shake_for(100, 10, NULL, NULL), "high sensitivity did not trigger");

	printf("8. size limit config is honoured\n");
	fresh(); config.shake_to_locate_max_size = 300; w = 0;
	shake_for(1200, 30, &w, NULL);
	CHECK(w > 0 && w <= 300, "width %d exceeds 300", w);
	printf("   max width with limit 300: %d\n", w);

	printf("9. default (no configured limit) respects hard cap 4096\n");
	fresh(); config.shake_to_locate_intensity = 20.0f; w = 0; /* grows fastest */
	shake_for(1500, 40, &w, NULL);
	CHECK(w <= 4096, "width %d > 4096", w);
	printf("   max width at max intensity: %d\n", w);

	printf("10. client cursor takes over mid-animation: animation stops, client image not touched\n");
	fresh(); shake_for(300, 20, NULL, NULL);
	CHECK(timer_armed() && cur.buffer, "not animating");
	int before = cur.set_buffer_calls;
	shake_to_locate_set_client_cursor();
	CHECK(!timer_armed(), "timer still armed");
	fire_timer();
	CHECK(cur.set_buffer_calls == before, "module drew over client cursor");
	
	shake_to_locate_set_cursor("left_ptr");

	printf("11. cursor hidden -> no trigger\n");
	fresh(); server.cursor_hidden = true;
	CHECK(!shake_for(300, 20, NULL, NULL) && !cur.buffer, "ran while hidden");

	printf("12. window move/resize grab is not a shake\n");
	fresh(); server.cursor_mode = CurMove;
	CHECK(!shake_for(300, 20, NULL, NULL), "triggered during grab");

	printf("13. shape change while animating re-renders once per new shape\n");
	fresh(); shake_for(300, 20, NULL, NULL);
	int c0 = cur.set_buffer_calls;
	for (int i = 0; i < 200; i++) shake_to_locate_set_cursor("left_ptr"); /* same shape: ignored */
	CHECK(cur.set_buffer_calls == c0, "same-shape calls re-rendered (%d)", cur.set_buffer_calls - c0);
	shake_to_locate_set_cursor("text"); /* not in the stub theme -> falls back, must not crash */
	shake_to_locate_set_cursor("grab");
	CHECK(cur.buffer != NULL, "lost enlarged cursor on shape change");
	settle(4000);
	CHECK(!strcmp(cur.xcursor_name, "grab") && !cur.buffer, "restored shape '%s'", cur.xcursor_name);

	printf("14. non-animating set_cursor behaves like wlr_cursor_set_xcursor\n");
	fresh(); int sc = cur.set_xcursor_calls; shake_to_locate_set_cursor("grab"); shake_to_locate_set_cursor("grab");
	CHECK(cur.set_xcursor_calls == sc + 1 && !strcmp(cur.xcursor_name, "grab"), "passthrough broken");

	printf("15. HiDPI: output scale 2 uses scale-2 reference, buffer scale passed to wlroots\n");
	fresh(); g_monitor_scale = 2.0f; w = 0;
	shake_for(200, 20, &w, NULL);
	CHECK(cur.buffer && cur.buffer_scale == 2.0f, "buffer scale %.1f", cur.buffer_scale);
	CHECK(cur.buffer && cur.buffer->width >= 48, "start width %d", cur.buffer ? cur.buffer->width : -1);
	g_monitor_scale = 1.0f; /* pointer moved to a 1x output while animating */
	shake_for(100, 20, NULL, NULL);
	CHECK(cur.buffer && cur.buffer_scale == 1.0f, "did not follow monitor scale (%.1f)", cur.buffer_scale);
	settle(4000);

	printf("16. reapply (theme reload) mid-animation: stops cleanly\n");
	fresh(); shake_for(300, 20, NULL, NULL);
	shake_to_locate_reapply();
	CHECK(!timer_armed(), "timer armed after reapply");
	fire_timer();
	shake_for(300, 20, NULL, NULL); /* can start again with a fresh source */
	CHECK(cur.buffer != NULL, "cannot animate after reapply");
	settle(4000);

	printf("17. theme without data: no crash, no animation\n");
	fresh(); wlr_xcursor_manager_destroy(server.cursor_manager); server.cursor_manager = wlr_xcursor_manager_create("test", 24);
	/* nothing loaded -> reference missing, source loads lazily and works; still must not crash */
	shake_for(300, 20, NULL, NULL); settle(4000);

	printf("18. finish during animation, then cursor teardown frees everything\n");
	fresh(); shake_for(300, 20, NULL, NULL);
	shake_to_locate_finish();
	CHECK(g_timer == NULL, "timer not removed");
	wlr_cursor_unset_image(&cur); /* wlr_cursor_destroy */
	CHECK(g_inits == g_destroys, "leak: created %d destroyed %d", g_inits, g_destroys);
	shake_to_locate_finish(); /* idempotent */

	if (server.cursor_manager) wlr_xcursor_manager_destroy(server.cursor_manager);
	printf("\n%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
