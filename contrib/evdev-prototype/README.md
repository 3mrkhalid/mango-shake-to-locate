# ShakeToLocate — recovered source module

This folder preserves the `shake_to_locate.c` source supplied in the conversation and a reconstructed public header.

## Contents

- `mango/input/shake_to_locate.c` — source exactly transcribed from the supplied code.
- `mango/input/shake_to_locate.h` — reconstructed declarations for the functions implemented by the source.

## Important recovery note

This is a recovered **module**, not a complete standalone Mango checkout. The C source depends on Mango internals (`server`, config, pointer helpers, and monitor types) and on wlroots/Wayland development headers. The original surrounding repository, build files, integration edits, and any other project files were not available in the conversation, so they are not fabricated here.

## Mango integration surface

The module exports:

- `shake_to_locate_init()`
- `shake_to_locate_finish()`
- `shake_to_locate_reapply()`
- `shake_to_locate_set_cursor(const char *name)`
- `shake_to_locate_motion(uint32_t time_msec, double dx, double dy)`

The implementation expects the Mango tree paths referenced by its includes to exist. Add this source to the compositor's existing build target and retain the call sites from the original Mango integration if you have them. No existing Mango files are modified by this archive.
