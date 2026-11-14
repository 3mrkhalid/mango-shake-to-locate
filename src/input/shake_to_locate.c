/*
 * ShakeToLocate - enlarge the cursor while the pointer is being shaken.
 *
 * Detection: movement distance + direction reversals inside a short window,
 * with a cooldown between activations.
 * Animation: a timer scales the theme's big cursor bitmap (bilinear) to any
 * size and hands it to wlroots as a plain buffer, so growth and shrinking are
 * continuous instead of stepping through preloaded cursor sizes.
 */
#include "mango/input/shake_to_locate.h"

#include <drm_fourcc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/xcursor.h>

#include "mango/common/server.h"
#include "mango/config/parse.h"
#include "mango/input/pointer.h"
#include "mango/manage/monitor.h"

/* ---- shake detection (distance / sensitivity / cooldown come from config) */
#define SHAKE_WINDOW_MS 500
#define SHAKE_MIN_REVERSALS 3
#define SHAKE_MIN_STEP 2.0

/* ---- animation */
#define SHAKE_FRAME_INTERVAL_MS 8
#define SHAKE_IDLE_MS 300
#define SHAKE_GROWTH_PX 100.0
#define SHAKE_DISTANCE_DECAY_MS 900.0
#define SHAKE_GROW_TAU_MS 180.0
#define SHAKE_SHRINK_TAU_MS 250.0
#define SHAKE_END_SCALE 1.02
#define SHAKE_SOURCE_SIZE 256
#define SHAKE_HARD_MAX_PX 4096.0
#define SHAKE_NAME_LEN 64


struct ShakeState {
  uint32_t window_start;
  uint32_t last_trigger;
  double distance;
  double last_dx;
  double last_dy;
  unsigned int reversals;
  bool triggered_once;
  bool active;
};

struct ShakeCursor {
  struct wlr_xcursor_manager *source; /* big theme bitmaps, created lazily */
  struct wl_event_source *timer;

  char name[SHAKE_NAME_LEN]; /* shape the compositor wants right now */
  bool client_cursor;        /* a client surface owns the cursor image */

  uint32_t last_motion;
  uint32_t last_frame;

  double motion_distance;
  double current_scale;
  double target_scale;
  double max_scale; /* from the last render, 0 = unknown */

  float loaded_scale; /* scale the source manager was loaded at */
  float last_ui_scale;
  int last_width;

  bool active;
  bool shrinking;
};

struct ScaledBuffer {
  struct wlr_buffer base;
  uint32_t *data;
  int width;
  int height;
};

static struct ShakeState state;
static struct ShakeCursor cursor_animation;

static uint32_t monotonic_msec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (uint32_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

/* ---------- custom wlr_buffer holding the scaled cursor bitmap ---------- */

static void scaled_buffer_destroy(struct wlr_buffer *wlr_buffer) {
  struct ScaledBuffer *buf = wl_container_of(wlr_buffer, buf, base);
  free(buf->data);
  free(buf);
}

static bool scaled_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
                                                uint32_t flags, void **data,
                                                uint32_t *format,
                                                size_t *stride) {
  struct ScaledBuffer *buf = wl_container_of(wlr_buffer, buf, base);

  if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
    return false;
  }

  *data = buf->data;
  *format = DRM_FORMAT_ARGB8888;
  *stride = (size_t)buf->width * 4;
  return true;
}

static void scaled_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
  (void)wlr_buffer;
}

static const struct wlr_buffer_impl scaled_buffer_impl = {
    .destroy = scaled_buffer_destroy,
    .begin_data_ptr_access = scaled_buffer_begin_data_ptr_access,
    .end_data_ptr_access = scaled_buffer_end_data_ptr_access,
};

/* Linear interpolation of two ARGB pixels, two 8-bit channels per step.
 * f is the weight of b in 0..256. A lane never exceeds 255 * 256 + 128, so
 * nothing carries between lanes. */
static inline uint32_t lerp_pixel(uint32_t a, uint32_t b, uint32_t f) {
  const uint32_t mask = 0x00ff00ffu;
  const uint32_t round = 0x00800080u;
  uint32_t even =
      (((a & mask) * (256 - f) + (b & mask) * f + round) >> 8) & mask;
  uint32_t odd =
      ((((a >> 8) & mask) * (256 - f) + ((b >> 8) & mask) * f + round) >> 8) &
      mask;

  return even | (odd << 8);
}

/* Two horizontally interpolated source rows are kept while the output rows
 * that need them are produced, so each source row is scaled only once. */
struct RowCache {
  int index[2];
  uint32_t *row[2];
};

static const uint32_t *row_cache_get(struct RowCache *rc, int want, int keep,
                                     const uint8_t *src, size_t src_stride,
                                     const int *x0, const int *x1,
                                     const uint32_t *tx, int dw) {
  for (int s = 0; s < 2; s++) {
    if (rc->index[s] == want) {
      return rc->row[s];
    }
  }

  int s = rc->index[0] == keep ? 1 : 0;
  const uint32_t *in = (const uint32_t *)(src + (size_t)want * src_stride);
  uint32_t *out = rc->row[s];

  for (int x = 0; x < dw; x++) {
    out[x] = lerp_pixel(in[x0[x]], in[x1[x]], tx[x]);
  }

  rc->index[s] = want;
  return out;
}

/* Bilinear scale of a premultiplied ARGB8888 bitmap (stride in bytes).
 * Returns false (dst untouched) when scratch memory is unavailable. */
static bool bilinear_scale(const uint8_t *src, size_t src_stride, int sw,
                           int sh, uint32_t *dst, int dw, int dh) {
  int *x0 = malloc(sizeof(int) * (size_t)dw);
  int *x1 = malloc(sizeof(int) * (size_t)dw);
  uint32_t *tx = malloc(sizeof(uint32_t) * (size_t)dw);
  uint32_t *row0 = malloc(sizeof(uint32_t) * (size_t)dw);
  uint32_t *row1 = malloc(sizeof(uint32_t) * (size_t)dw);

  if (!x0 || !x1 || !tx || !row0 || !row1) {
    free(x0);
    free(x1);
    free(tx);
    free(row0);
    free(row1);
    return false;
  }

  struct RowCache rc = {.index = {-1, -1}, .row = {row0, row1}};
  double xr = (double)sw / dw;
  double yr = (double)sh / dh;

  for (int x = 0; x < dw; x++) {
    double fx = (x + 0.5) * xr - 0.5;
    if (fx < 0.0)
      fx = 0.0;
    int ix = (int)fx;
    if (ix > sw - 1)
      ix = sw - 1;
    x0[x] = ix;
    x1[x] = ix + 1 < sw ? ix + 1 : ix;
    tx[x] = (uint32_t)((fx - ix) * 256.0);
  }

  for (int y = 0; y < dh; y++) {
    double fy = (y + 0.5) * yr - 0.5;
    if (fy < 0.0)
      fy = 0.0;
    int iy = (int)fy;
    if (iy > sh - 1)
      iy = sh - 1;
    int iy1 = iy + 1 < sh ? iy + 1 : iy;
    uint32_t ty = (uint32_t)((fy - iy) * 256.0);

    const uint32_t *h0 =
        row_cache_get(&rc, iy, iy1, src, src_stride, x0, x1, tx, dw);
    const uint32_t *h1 =
        row_cache_get(&rc, iy1, iy, src, src_stride, x0, x1, tx, dw);
    uint32_t *out = dst + (size_t)y * dw;

    for (int x = 0; x < dw; x++) {
      out[x] = lerp_pixel(h0[x], h1[x], ty);
    }
  }

  free(x0);
  free(x1);
  free(tx);
  free(row0);
  free(row1);
  return true;
}

/* ---------- cursor helpers ---------- */

static struct wlr_xcursor_image *get_image(struct wlr_xcursor_manager *manager,
                                           const char *name, float scale) {
  if (!manager || !name) {
    return NULL;
  }

  struct wlr_xcursor *xcursor =
      wlr_xcursor_manager_get_xcursor(manager, name, scale);

  if (!xcursor || xcursor->image_count == 0) {
    return NULL;
  }

  return xcursor->images[0];
}

/* Scale of the output the pointer is on (buffer scale of the cursor). */
static float cursor_ui_scale(void) {
  Monitor *m = monitor_at_point(server.cursor->x, server.cursor->y);

  if (m && m->wlr_output && m->wlr_output->scale > 0.f) {
    return m->wlr_output->scale;
  }

  return 1.0f;
}

static bool ensure_source(float ui_scale) {
  if (!cursor_animation.source) {
    cursor_animation.source =
        wlr_xcursor_manager_create(config.cursor_theme, SHAKE_SOURCE_SIZE);
    cursor_animation.loaded_scale = 0.0f;

    if (!cursor_animation.source) {
      return false;
    }
  }

  if (cursor_animation.loaded_scale != ui_scale) {
    wlr_xcursor_manager_load(cursor_animation.source, ui_scale);
    cursor_animation.loaded_scale = ui_scale;
  }

  return true;
}

static void drop_source(void) {
  if (cursor_animation.source) {
    wlr_xcursor_manager_destroy(cursor_animation.source);
    cursor_animation.source = NULL;
  }

  cursor_animation.loaded_scale = 0.0f;
}

static double size_limit_px(void) {
  double limit = SHAKE_HARD_MAX_PX;

  if (config.shake_to_locate_max_size > 0 &&
      config.shake_to_locate_max_size < limit) {
    limit = config.shake_to_locate_max_size;
  }

  return limit;
}

static const char *current_shape(void) {
  return cursor_animation.name[0] ? cursor_animation.name : "left_ptr";
}

/* Put the normal theme cursor back. wlr_cursor_set_xcursor() ignores a name
 * it believes is already set, so clear the image first to be sure that the
 * enlarged buffer is really replaced. Both calls happen in the same event
 * loop dispatch, before the next frame. */
static void restore_normal_cursor(void) {
  if (!server.cursor || !server.cursor_manager || server.cursor_hidden ||
      cursor_animation.client_cursor) {
    return;
  }

  wlr_cursor_unset_image(server.cursor);
  wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, current_shape());
}

/* Draw the cursor at an arbitrary (continuous) scale factor. */
static void render_cursor(double scale, bool force) {
  if (!server.cursor || server.cursor_hidden ||
      cursor_animation.client_cursor) {
    return;
  }

  const char *name = current_shape();
  float ui_scale = cursor_ui_scale();

  if (!ensure_source(ui_scale)) {
    return;
  }

  struct wlr_xcursor_image *src =
      get_image(cursor_animation.source, name, ui_scale);
  if (!src) {
    name = "left_ptr";
    src = get_image(cursor_animation.source, name, ui_scale);
  }
  if (!src || src->width == 0 || src->height == 0) {
    return;
  }

  /* The size the cursor normally has is the reference for scale 1.0 */
  struct wlr_xcursor_image *ref =
      get_image(server.cursor_manager, name, ui_scale);
  double base_width =
      ref ? (double)ref->width
          : (config.cursor_size > 0 ? (double)config.cursor_size : 24.0) *
                ui_scale;
  if (base_width < 1.0) {
    base_width = 1.0;
  }

  /* Largest scale that keeps the bitmap inside the size limit. */
  double aspect = fmax(1.0, (double)src->height / (double)src->width);
  double limit = size_limit_px();
  cursor_animation.max_scale = fmax(1.0, limit / aspect / base_width);

  if (scale > cursor_animation.max_scale) {
    scale = cursor_animation.max_scale;
  }

  double dw_f = fmax(1.0, base_width * scale);
  double ratio = dw_f / src->width;
  double dh_f = fmax(1.0, src->height * ratio);

  int dw = (int)lround(dw_f);
  int dh = (int)lround(dh_f);
  if (dw < 1)
    dw = 1;
  if (dh < 1)
    dh = 1;

  if (!force && dw == cursor_animation.last_width &&
      ui_scale == cursor_animation.last_ui_scale) {
    return; /* nothing visible changed */
  }

  struct wlr_buffer *source_buffer = wlr_xcursor_image_get_buffer(src);
  if (!source_buffer) {
    return;
  }

  void *src_data = NULL;
  uint32_t src_format = 0;
  size_t src_stride = 0;
  if (!wlr_buffer_begin_data_ptr_access(source_buffer,
                                        WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                        &src_data, &src_format, &src_stride)) {
    return;
  }

  struct ScaledBuffer *buf = calloc(1, sizeof(*buf));
  bool ok = buf && src_format == DRM_FORMAT_ARGB8888;

  if (ok) {
    buf->data = malloc((size_t)dw * (size_t)dh * 4);
    ok = buf->data && bilinear_scale(src_data, src_stride, (int)src->width,
                                     (int)src->height, buf->data, dw, dh);
  }

  wlr_buffer_end_data_ptr_access(source_buffer);

  if (!ok) {
    if (buf) {
      free(buf->data);
      free(buf);
    }
    return;
  }

  buf->width = dw;
  buf->height = dh;
  wlr_buffer_init(&buf->base, &scaled_buffer_impl, dw, dh);

  int hot_x = (int)lround(src->hotspot_x * ratio);
  int hot_y = (int)lround(src->hotspot_y * ratio);

  wlr_cursor_set_buffer(server.cursor, &buf->base, hot_x, hot_y, ui_scale);

  /* wlroots keeps its own lock on the buffer; ours is released here and
   * the buffer is freed by wlroots when it is replaced or the cursor goes
   * away. */
  wlr_buffer_drop(&buf->base);

  cursor_animation.last_width = dw;
  cursor_animation.last_ui_scale = ui_scale;
}

static void stop_animation(void) {
  cursor_animation.active = false;
  cursor_animation.shrinking = false;
  cursor_animation.last_width = 0;
  cursor_animation.last_ui_scale = 0.0f;
  cursor_animation.motion_distance = 0.0;

  if (cursor_animation.timer) {
    wl_event_source_timer_update(cursor_animation.timer, 0);
  }
}

static int shake_cursor_timer_callback(void *data) {
  (void)data;

  if (!cursor_animation.active) {
    return 0;
  }

  if (!config.shake_to_locate || server.cursor_hidden ||
      cursor_animation.client_cursor) {
    /* A client or a hide request took over the image: do not touch it. */
    bool restore = !config.shake_to_locate && !server.cursor_hidden;
    stop_animation();
    if (restore) {
      restore_normal_cursor();
    }
    return 0;
  }

  uint32_t now = monotonic_msec();
  double dt = (double)(now - cursor_animation.last_frame);
  if (dt < 1.0)
    dt = 1.0;
  if (dt > 100.0)
    dt = 100.0;
  cursor_animation.last_frame = now;

  double growth_px = SHAKE_GROWTH_PX / (double)config.shake_to_locate_intensity;

  /* the "shake energy" fades, so you must keep shaking to keep growing */
  cursor_animation.motion_distance *= exp(-dt / SHAKE_DISTANCE_DECAY_MS);

  /* do not wind up energy beyond what the size limit can show */
  if (cursor_animation.max_scale >= 1.0) {
    double max_energy = (cursor_animation.max_scale - 1.0) * growth_px;
    if (cursor_animation.motion_distance > max_energy) {
      cursor_animation.motion_distance = max_energy;
    }
  }

  if (now - cursor_animation.last_motion >= SHAKE_IDLE_MS) {
    cursor_animation.shrinking = true;
  }

  if (cursor_animation.shrinking) {
    cursor_animation.target_scale = 1.0;
  } else {
    cursor_animation.target_scale =
        1.0 + cursor_animation.motion_distance / growth_px;
  }

  /* frame-rate independent exponential smoothing */
  double tau = cursor_animation.target_scale > cursor_animation.current_scale
                   ? SHAKE_GROW_TAU_MS
                   : SHAKE_SHRINK_TAU_MS;
  double alpha = 1.0 - exp(-dt / tau);

  cursor_animation.current_scale +=
      (cursor_animation.target_scale - cursor_animation.current_scale) * alpha;

  if (cursor_animation.shrinking &&
      cursor_animation.current_scale < SHAKE_END_SCALE) {
    stop_animation();
    restore_normal_cursor();
    return 0;
  }

  render_cursor(cursor_animation.current_scale, false);

  wl_event_source_timer_update(cursor_animation.timer, SHAKE_FRAME_INTERVAL_MS);

  return 0;
}

void shake_to_locate_reapply(void) {
  /* The caller has just recreated the cursor manager and set a fresh
   * cursor, so there is nothing to restore: just forget the old state. */
  stop_animation();
  drop_source();

  snprintf(cursor_animation.name, sizeof(cursor_animation.name), "%s",
           "left_ptr");
  cursor_animation.client_cursor = false;
  cursor_animation.current_scale = 1.0;
  cursor_animation.target_scale = 1.0;
  cursor_animation.max_scale = 0.0;
  state.active = false;
}

void shake_to_locate_set_cursor(const char *name) {
  if (!name || !server.cursor) {
    return;
  }

  cursor_animation.client_cursor = false;

  if (cursor_animation.active) {
    /* Called on nearly every motion event: only react to a new shape. */
    if (strcmp(cursor_animation.name, name) != 0) {
      snprintf(cursor_animation.name, sizeof(cursor_animation.name), "%s",
               name);
      render_cursor(cursor_animation.current_scale, true);
    }
    return;
  }

  snprintf(cursor_animation.name, sizeof(cursor_animation.name), "%s", name);

  if (server.cursor_manager) {
    wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, name);
  }
}

void shake_to_locate_set_client_cursor(void) {
  cursor_animation.client_cursor = true;

  if (cursor_animation.active) {
    /* The client image is applied right after this call. */
    stop_animation();
  }
}

void shake_to_locate_init(void) {
  if (cursor_animation.timer) {
    return;
  }

  memset(&state, 0, sizeof(state));
  memset(&cursor_animation, 0, sizeof(cursor_animation));
  snprintf(cursor_animation.name, sizeof(cursor_animation.name), "%s",
           "left_ptr");

  if (server.event_loop) {
    cursor_animation.timer = wl_event_loop_add_timer(
        server.event_loop, shake_cursor_timer_callback, NULL);
  }
}

void shake_to_locate_finish(void) {
  stop_animation();

  if (cursor_animation.timer) {
    wl_event_source_remove(cursor_animation.timer);
    cursor_animation.timer = NULL;
  }

  drop_source();
  memset(&state, 0, sizeof(state));
}

bool shake_to_locate_motion(uint32_t time_msec, double dx, double dy) {
  if (!config.shake_to_locate || !time_msec || (!dx && !dy)) {
    return false;
  }

  if (cursor_animation.active) {
    cursor_animation.last_motion = monotonic_msec();
    cursor_animation.shrinking = false;
    cursor_animation.motion_distance += hypot(dx, dy);
    return false;
  }

  /* Window moves/resizes are not shakes. */
  if (server.cursor_mode != CurNormal) {
    state.active = false;
    return false;
  }

  if (!state.active || time_msec - state.window_start > SHAKE_WINDOW_MS) {
    state.window_start = time_msec;
    state.distance = 0.0;
    state.reversals = 0;
    state.last_dx = 0.0;
    state.last_dy = 0.0;
    state.active = true;
  }

  double step = hypot(dx, dy);
  state.distance += step;

  if (step >= SHAKE_MIN_STEP) {
    double dot = dx * state.last_dx + dy * state.last_dy;

    if ((state.last_dx != 0.0 || state.last_dy != 0.0) && dot < 0.0) {
      state.reversals++;
    }

    state.last_dx = dx;
    state.last_dy = dy;
  }

  double min_distance = (double)config.shake_to_locate_min_distance /
                        (double)config.shake_to_locate_sensitivity;

  if (state.distance >= min_distance &&
      state.reversals >= SHAKE_MIN_REVERSALS) {

    if (!state.triggered_once ||
        time_msec - state.last_trigger >= config.shake_to_locate_cooldown) {

      state.last_trigger = time_msec;
      state.triggered_once = true;
      state.active = false;

      if (cursor_animation.timer && server.cursor && server.cursor_manager &&
          !server.cursor_hidden && !cursor_animation.client_cursor) {

        uint32_t now = monotonic_msec();

        cursor_animation.active = true;
        cursor_animation.shrinking = false;
        cursor_animation.motion_distance = 0.0;
        cursor_animation.current_scale = 1.0;
        cursor_animation.target_scale = 1.0;
        cursor_animation.last_width = 0;
        cursor_animation.last_ui_scale = 0.0f;
        cursor_animation.last_motion = now;
        cursor_animation.last_frame = now;

        render_cursor(1.0, true);

        /* Nothing could be drawn (no theme data): do not animate. */
        if (cursor_animation.last_width == 0) {
          stop_animation();
          return false;
        }

        wl_event_source_timer_update(cursor_animation.timer,
                                     SHAKE_FRAME_INTERVAL_MS);
        return true;
      }
    }
  }

  return false;
}
