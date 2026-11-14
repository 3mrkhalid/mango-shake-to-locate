# ShakeToLocate for Mango

Shake the mouse and the cursor grows so you can find it, then shrinks back when
you stop. It is the same idea as "shake to locate pointer" on macOS or KDE.

This project adds the feature to the [Mango](https://github.com/mangowm/mango)
Wayland compositor. You do not edit any source code: run one script.

> **Status: untested on a real Mango build.** The code was written against the actual
> Mango source (commit `ba0c8d9`) and its logic is covered by automated tests, but it
> has **not been compiled against Mango/wlroots 0.20 or run on a real desktop** by the
> author of this package. Read [Testing](#testing) before relying on it.

*No screenshots or demo recording yet: none could be produced without a running Mango.*

## Install

```sh
git clone https://github.com/3mrkhalid/mango-shake-to-locate
cd mango-shake-to-locate
./install.sh --check-only     # lists missing dependencies, changes nothing
./install.sh                  # builds and installs
```

The installer builds a **separate copy** of Mango and installs it into
`~/.local/share/mango-shake-to-locate` with a launcher `~/.local/bin/mango-shake`.
Your existing Mango and your configuration are **never modified**.

Start it from a TTY (log out of your current Mango session first):

```sh
mango-shake
```

To use it from a display manager, create a session entry whose `Exec` is
`~/.local/share/mango-shake-to-locate/bin/mango` (use the absolute path).

It is enabled by default. Shake the mouse left and right quickly.

## Configure

Add any of these to `~/.config/mango/config.conf` (a ready-to-copy list is installed in
`~/.local/share/mango-shake-to-locate/share/mango-shake-to-locate/shake-to-locate.conf.example`),
then reload the Mango configuration. Defaults shown.

| Option | Default | Meaning |
|---|---|---|
| `shake_to_locate` | `1` | `1` on, `0` off |
| `shake_to_locate_sensitivity` | `1.0` | higher triggers with less movement (0.1 - 10) |
| `shake_to_locate_min_distance` | `90` | pixels of travel within 500 ms before a shake counts (10 - 5000), divided by the sensitivity |
| `shake_to_locate_intensity` | `1.0` | how fast and how big the cursor grows (0.1 - 20) |
| `shake_to_locate_cooldown` | `1200` | minimum ms between two activations (0 - 60000) |
| `shake_to_locate_max_size` | `0` | largest cursor size in px; `0` = no configured limit (a hard cap of 4096 px always applies) |

A shake is also required to include at least 3 direction reversals, and it is ignored
while moving or resizing a window, while the cursor is hidden, and while a client has
set its own cursor image (games, drawing apps).

## Uninstall

```sh
./uninstall.sh            # removes the install directory, launcher and build cache
./uninstall.sh --dry-run  # only shows what would be removed
```

It only deletes directories that carry this installer's marker file and never touches
`~/.config/mango`.

## Requirements and supported versions

- Supported: Mango commit **`ba0c8d9859f9142d1cfcc61f39f289c545661fb9`** (0.18.5 plus 15
  commits, from `DreamMaoMao/mangowc`, which is also published as `mangowm/mango`).
  This is the only version the integration patch is written and checked against. The
  installer stops with a clear message if the patch does not apply.
- Build tools: `meson`, `ninja`, a C compiler, `pkg-config`, `patch`, `wayland-scanner`, `git`.
- Libraries (as required by that Mango commit): wayland >= 1.23.1, **wlroots 0.20**,
  **scenefx 0.5**, libdrm, xkbcommon, libinput >= 1.27.1, pixman, pcre2, cjson,
  pangocairo, and xcb/xcb-icccm/xcb-randr for XWayland (or pass `--no-xwayland`).
  `./install.sh --check-only` reports exactly which are missing.
- Mango's own dependency/build notes: its `docs/installation.md`.

## How it works

See [docs/INTEGRATION.md](docs/INTEGRATION.md) for every Mango source change and why.
In short: relative pointer motion is fed to a detector (distance, direction reversals,
cooldown). When it fires, a timer scales the theme's large cursor bitmap with bilinear
interpolation to any size and hands it to wlroots as a buffer, so growth and shrinking
are continuous instead of stepping through preloaded cursor sizes. When the animation
ends the normal themed cursor is restored.

## Limitations

- **Single build, pinned commit.** Other Mango versions are not supported.
- **Software scaling on the CPU.** Cost grows with cursor size (measured on the
  author's build machine: about 2 ms/frame at 1024 px, 10 ms at 2048 px, 44 ms at
  4096 px). Because Mango is single-threaded, very large sizes can make the compositor
  feel sluggish while the cursor is huge. Use `shake_to_locate_max_size` to limit it.
- Very large cursors cannot use the hardware cursor plane, so wlroots draws them in the
  compositor (software cursor).
- The theme's 256 px bitmap is loaded on the first shake (a short pause once), and kept
  in memory until the config is reloaded.
- At the very start and end of the animation the cursor is downscaled from the large
  bitmap with plain bilinear filtering, which can look slightly rougher than the theme's
  own small cursor for a few frames.
- Client-provided cursors (a Wayland client drawing its own cursor surface) are not
  enlarged.
- Animated cursors are shown as their first frame while enlarged.
- No screenshots/demo are included.

## Testing

What was actually done (on the packaging machine, which has no wlroots/Mango dependencies):

- The integration patch applies cleanly to the pinned Mango commit (`patch --dry-run`,
  `git apply --check`), and the installer's fetch → patch → copy stage ran end to end.
- `make test` builds the module against **stub** wlroots/Mango headers
  (`tests/stubs`) with `-Wall -Wextra -Werror` and AddressSanitizer + UBSan and runs
  235 checks: detection, cooldown, config options, continuous growth, restore (also
  when wlroots keeps its cursor name cached), client-cursor takeover, shape changes,
  output scale changes, reapply mid-animation, finish/teardown, and buffer
  create/destroy balance. A mutation check confirmed that breaking the restore and the
  buffer release makes these tests fail. A second test compares the scaler with a
  floating-point bilinear reference (worst error 2.4 of 255).
- The stubs model how I expect wlroots to behave (buffer locking, cursor-name
  de-duplication, scale lookup). **They are not wlroots.**

What was **not** done, and must be done on a machine with the dependencies:

1. Build against the real Mango/wlroots 0.20/scenefx 0.5 (`./install.sh`).
2. Run it: shake, check growth, shrink and that the normal cursor comes back.
3. Test with two or more monitors, with different output scales (for example 1x and
   2x), and move the pointer across monitors while it is enlarged.
4. Check the cursor over client cursors (text fields, resize edges), while dragging
   windows, with a game or a client that hides the cursor, and after reloading the
   configuration or changing the cursor theme/size.

Please report results (and your Mango/wlroots versions) so this section can be updated.

## Repository layout

```
install.sh / uninstall.sh   user-facing scripts
scripts/common.sh           shared helpers
src/, include/              the ShakeToLocate module (new Mango files)
patches/                    edits to existing Mango files (applied by install.sh)
config/                     example configuration
tests/                      stub-based tests (make test)
docs/INTEGRATION.md         what changes in Mango and why
contrib/evdev-prototype/    the original standalone C++ evdev prototype, kept untouched,
                            not part of the Mango feature and not installed
LICENSING.md                licensing situation (no LICENSE file yet)
```

## License

See [LICENSING.md](LICENSING.md). A license file has deliberately not been added yet.
