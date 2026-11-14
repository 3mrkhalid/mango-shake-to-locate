# Mango integration (commit ba0c8d9)

New files (copied by the installer): `src/input/shake_to_locate.c`,
`include/mango/input/shake_to_locate.h`. Mango's `meson.build` already globs
`src/**/*.c`, so no build-file change is needed (`libdrm` and `-DWLR_USE_UNSTABLE`, which the
module needs for `drm_fourcc.h` and the custom `wlr_buffer`, are already used by Mango).

Edits to existing files (`patches/0001-*.patch`):

| File | Change | Why |
|---|---|---|
| `src/input/pointer.c` | `shake_to_locate_motion(time, dx, dy)` right after `wlr_cursor_move()` in `pointer_process_motion` | Feeds relative motion after the cursor really moved. Locked/confined pointers return earlier, so games are unaffected |
| `src/input/pointer.c` | `wlr_cursor_set_xcursor(...)` → `shake_to_locate_set_cursor(name)` at the compositor's shape-setting sites (shape request, resize-edge hover, grab, default, activity restore, monitor warp) | Identical behaviour when idle; while the cursor is enlarged, a shape change re-renders the enlarged cursor instead of being overwritten |
| `src/input/pointer.c`, `src/input/tablet.c` | `shake_to_locate_set_client_cursor()` before client-surface cursors are applied | The module must know when a client owns the image (including client-hidden cursors) so it never draws over it |
| `src/manage/misc.c`, `src/dispatch/bind.c` | same `set_cursor` replacement for the `"default"` shape | keeps the module's idea of the current shape correct |
| `src/main.c` | `shake_to_locate_init()` after `hide_cursor_source` is created; `shake_to_locate_finish()` before `wlr_xcursor_manager_destroy(server.cursor_manager)` in cleanup | timer lifecycle |
| `src/config/reapply.c` | `shake_to_locate_reapply()` at the end of `reapply_cursor_style()` | theme/size/config change: stop animation, drop cached theme data |
| `include/mango/config/parse.h`, `src/config/option_defs.c` | six `shake_to_locate*` options: struct fields, parsing, defaults, clamping | follows Mango's existing option conventions |
| `docs/configuration/miscellaneous.md` | documents the options | |

## Differences from the recovered module

- The recovered code called `pointer_has_client_cursor()`, which **does not exist** in
  Mango. Mango's `last_cursor` reflects the last client request, not what is on screen,
  so it cannot answer that question. The module now tracks it itself through
  `shake_to_locate_set_cursor()` / `shake_to_locate_set_client_cursor()`.
- The recovered header is replaced (adds `set_client_cursor`).
- Source pixels are read through `wlr_xcursor_image_get_buffer()` (the API Mango already
  uses), not a raw image pointer, and the stride is honoured.
- The 256 px theme data is created lazily and loaded for the output scale under the
  pointer, so mixed-scale multi-monitor setups follow the pointer.
- Restore clears the cursor image before setting the theme cursor, because
  `wlr_cursor_set_xcursor()` can ignore a name it believes is already set.
- Detection is not active during window move/resize.
- The scaler was rewritten (row cache + two-lane interpolation) after measuring 173 ms
  per 4096 px frame with the straightforward version; it is now about 44 ms.
- Settings that were `#define`s (distance, cooldown, growth, size limit) are now config
  options. The remaining constants stay in the source.
