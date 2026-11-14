#ifndef __INPUT_SHAKE_TO_LOCATE_H__
#define __INPUT_SHAKE_TO_LOCATE_H__

#include <stdbool.h>
#include <stdint.h>

/* ShakeToLocate: enlarge the cursor while the pointer is being shaken. */

/* Create the animation timer. Call once, after server.event_loop exists. */
void shake_to_locate_init(void);

/* Stop the animation and release everything. Call before the cursor manager
 * is destroyed. Safe to call without a previous init. */
void shake_to_locate_finish(void);

/* Call after the cursor theme/size changed (cursor manager recreated) or the
 * config was reloaded. Stops a running animation and drops cached theme data.
 */
void shake_to_locate_reapply(void);

/* Replacement for wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
 * name). Identical behaviour while no animation runs; while it runs, the
 * enlarged cursor switches to the new shape instead of being overwritten. */
void shake_to_locate_set_cursor(const char *name);

/* Call whenever a client-provided cursor surface is (or is about to be) set
 * on the cursor, including a NULL surface (client-hidden cursor). Client
 * surfaces cannot be scaled, so ShakeToLocate leaves them alone. */
void shake_to_locate_set_client_cursor(void);

/* Feed relative pointer motion (after the cursor has moved). Returns true when
 * this event triggered a new activation. */
bool shake_to_locate_motion(uint32_t time_msec, double dx, double dy);

#endif
