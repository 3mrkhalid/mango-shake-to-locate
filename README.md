# ShakeToLocate for Mango

Shake your mouse and the cursor grows so you can find it, then smoothly shrinks back when you stop. Inspired by the "Shake to Locate Pointer" feature in macOS and KDE.

ShakeToLocate adds this feature to the [Mango](https://github.com/mangowm/mango) Wayland compositor. It integrates directly with Mango through a small source patch and an automated installer.

---

> **Build verified, runtime testing still needed.** The integration has been built successfully against the pinned Mango source on the author's machine. The feature's logic is also covered by automated tests. However, it has not yet been fully validated through real desktop use across different applications, monitors, and cursor configurations. See [Testing](#testing) before relying on it.

## Demo

[https://github.com/user-attachments/assets/7d10b9eb-ba7e-43e9-a19c-3a9a3de27d37](https://github.com/user-attachments/assets/fa33d978-43ae-4b00-a71c-02fb291ea9e7)

## Install

```sh
git clone https://github.com/3mrkhalid/mango-shake-to-locate
cd mango-shake-to-locate

./install.sh --check-only     # Check dependencies without changing anything
./install.sh                  # Build and install
```

The installer builds a **separate copy** of Mango and installs it into:

```text
~/.local/share/mango-shake-to-locate
```

It also creates a launcher at:

```text
~/.local/bin/mango-shake
```

Your existing Mango executable and configuration are **not modified** by the installer.

## Run ShakeToLocate

Log out of your current Mango session first, then start the new build from a TTY:

```sh
~/.local/share/mango-shake-to-locate/bin/mango
```

Alternatively, if `~/.local/bin` is in your `PATH`, you can use the launcher:

```sh
mango-shake
```

To use it from a display manager, create a session entry whose `Exec` points to the absolute path of the installed executable:

```text
/home/your-user/.local/share/mango-shake-to-locate/bin/mango
```

Replace `/home/your-user` with your actual home directory.

ShakeToLocate is enabled by default. Move the mouse quickly from side to side to activate it.

## Use as your default Mango

If you want the ShakeToLocate build to launch whenever you run the regular `mango` command, you can replace your currently installed Mango executable with the new build.

This is optional. The normal installation does not replace your existing Mango executable.

**1. Find your current Mango executable:**

```bash
MANGO_BIN="$(command -v mango)"
echo "$MANGO_BIN"
readlink -f "$MANGO_BIN"
```

Make sure this points to the Mango executable you actually want to replace.

**2. Back up the original executable:**

```bash
sudo cp -L -- "$MANGO_BIN" "$MANGO_BIN.backup"
```

The `-L` option follows a symbolic link and backs up the executable it points to, rather than saving the link itself.

**Important:** Verify that the resolved executable is your original Mango build before creating the backup. If it already points to the ShakeToLocate installation, do not use it as your original backup.

**3. Replace it with the ShakeToLocate build:**

```bash
sudo rm -f -- "$MANGO_BIN"

sudo install -m 755 \
  ~/.local/share/mango-shake-to-locate/bin/mango \
  "$MANGO_BIN"
```

**4. Log out and start Mango again.**

Your regular Mango command will now launch the ShakeToLocate build:

```bash
mango
```

**Restore your original Mango executable:**

```bash
sudo rm -f -- "$MANGO_BIN"

sudo install -m 755 \
  "$MANGO_BIN.backup" \
  "$MANGO_BIN"
```

This method replaces the installed executable; it does not merge the changes into your original Mango source repository.

Keep your backup until you are sure everything works. The replacement is based on the pinned Mango commit documented below.

## Configure

Add any of these options to:

```text
~/.config/mango/config.conf
```

A ready-to-copy configuration file is installed at:

```text
~/.local/share/mango-shake-to-locate/share/mango-shake-to-locate/shake-to-locate.conf.example
```

Reload the Mango configuration after changing the options.

| **Option** | **Default** | **Meaning** |
| ------------------------------ | ----------- | -------------------------------------------------------------------------------------------- |
| `shake_to_locate` | `1` | `1` enables the feature, `0` disables it |
| `shake_to_locate_sensitivity` | `1.0` | Higher values trigger with less movement (0.1 - 10) |
| `shake_to_locate_min_distance` | `90` | Pixels of travel within 500 ms before a shake counts (10 - 5000), adjusted by sensitivity |
| `shake_to_locate_intensity` | `1.0` | Controls cursor growth speed; higher values make the cursor grow faster (0.1 - 20) |
| `shake_to_locate_cooldown` | `1200` | Minimum milliseconds between two activations (0 - 60000) |
| `shake_to_locate_max_size` | `0` | Largest cursor size in pixels; `0` means no configured limit (a hard cap of 4096 px always applies) |

A valid shake must include at least **3 direction reversals**.

The effect is ignored while:

- Moving or resizing a window.
- The cursor is hidden.
- A Wayland client has set its own cursor image, such as some games and drawing applications.

## Uninstall

From the cloned project directory:

```sh
./uninstall.sh            # Remove the installation, launcher and build cache
./uninstall.sh --dry-run  # Preview what would be removed
```

The uninstaller only deletes directories containing this installer's marker file. It never touches:

```text
~/.config/mango
```

If you manually replaced your default Mango executable using the instructions above, restore your original executable before uninstalling the ShakeToLocate installation.

## Requirements and supported versions

- **Supported Mango version:** Commit `ba0c8d9859f9142d1cfcc61f39f289c545661fb9` (Mango 0.18.5 plus 15 commits, from `DreamMaoMao/mangowc`, also published as `mangowm/mango`).
- This is the only version the integration patch is written and checked against. The installer stops with a clear message if the patch does not apply.
- **Build tools:** `meson`, `ninja`, a C compiler, `pkg-config`, `patch`, `wayland-scanner`, and `git`.
- **Libraries:** Wayland >= 1.23.1, **wlroots 0.20**, **scenefx 0.5**, libdrm, xkbcommon, libinput >= 1.27.1, pixman, pcre2, cjson, pangocairo, and xcb/xcb-icccm/xcb-randr for XWayland.
- You can pass `--no-xwayland` to disable XWayland support during the build.
- Run `./install.sh --check-only` to check which dependencies are missing.
- For Mango's own dependency and build instructions, see its `docs/installation.md`.

## How it works

See [docs/INTEGRATION.md](docs/INTEGRATION.md) for the Mango source changes and the reasons behind them.

In short:

1. Relative pointer motion is passed to a shake detector.
2. The detector checks movement distance, direction reversals, and cooldown.
3. When a shake is detected, a timer drives the cursor animation.
4. The cursor's large theme bitmap is scaled using bilinear interpolation.
5. The resulting image is provided to wlroots as a buffer.
6. When the animation ends, the normal themed cursor is restored.

Instead of stepping through a few preloaded cursor sizes, the feature continuously scales the cursor, allowing smooth growth and shrinking.

## Limitations

- **Single supported build:** Only the pinned Mango commit is currently supported.
- **CPU-based scaling:** Cursor scaling is performed on the CPU. The measured cost on the author's build machine was approximately:
  - 2 ms/frame at 1024 px.
  - 10 ms/frame at 2048 px.
  - 44 ms/frame at 4096 px.

  Mango is single-threaded, so very large cursor sizes may make the compositor feel sluggish while the cursor is enlarged. Use `shake_to_locate_max_size` to limit the maximum size.
- **Software cursor:** Very large cursors cannot use the hardware cursor plane, so wlroots draws them in the compositor.
- **First-shake loading:** The theme's 256 px bitmap is loaded on the first shake and kept in memory until the configuration is reloaded. This may cause a short pause the first time.
- **Scaling quality:** At the beginning and end of the animation, the cursor is downscaled from the large bitmap using bilinear filtering. This can look slightly rougher than the theme's original small cursor for a few frames.
- **Client-provided cursors:** Wayland clients drawing their own cursor surfaces are not enlarged.
- **Animated cursors:** Animated cursor themes are displayed using their first frame while enlarged.
- **No screenshots or demo recording** are currently included.

## Testing

### What has been tested

- The integration patch applies cleanly to the pinned Mango commit using `patch --dry-run` and `git apply --check`.
- The installer's fetch, patch, build, and installation process has been run successfully on the author's machine.
- `make test` builds the module against **stub** wlroots/Mango headers in `tests/stubs`, using `-Wall -Wextra -Werror`, AddressSanitizer, and UndefinedBehaviorSanitizer.
- The automated test suite runs 235 checks covering:
  - Shake detection and cooldown.
  - Configuration options.
  - Continuous cursor growth.
  - Cursor restoration, including when wlroots keeps its cursor name cached.
  - Client-cursor takeover.
  - Cursor shape changes.
  - Output scale changes.
  - Reapplying configuration during animation.
  - Cleanup and teardown.
  - Buffer creation and destruction balance.
- A mutation check confirmed that breaking cursor restoration or buffer release causes the tests to fail.
- A second test compares the scaler against a floating-point bilinear reference, with a worst error of 2.4 out of 255.

**Important:** The stubs model expected wlroots behavior, including buffer locking, cursor-name deduplication, and scale lookup. They are not the real wlroots implementation.

### What still needs real-world testing

The following checks should be performed on a machine running the supported Mango and wlroots versions:

1. Run ShakeToLocate and verify that the cursor grows, shrinks, and returns to its normal appearance.
2. Test with two or more monitors using different output scales, such as 1x and 2x. Move the pointer between monitors while the cursor is enlarged.
3. Check cursor behavior over text fields, resize edges, and other client-provided cursor surfaces.
4. Test while dragging windows, using games, and interacting with clients that hide the cursor.
5. Reload the configuration and change the cursor theme or size while testing the feature.

Please report your results, including your Mango and wlroots versions, so this section can be updated.

## Repository layout

```text
install.sh / uninstall.sh   User-facing installation scripts
scripts/common.sh           Shared helper functions
src/, include/              ShakeToLocate module (new Mango files)
patches/                    Changes to existing Mango files
config/                     Example configuration
tests/                      Stub-based tests (make test)
docs/INTEGRATION.md         Mango integration details
contrib/evdev-prototype/     Original standalone C++ evdev prototype
                            Kept untouched; not part of the Mango feature
                            and not installed
LICENSING.md                Licensing information
```

## License

See [LICENSING.md](LICENSING.md) for the current licensing situation.

A `LICENSE` file has deliberately not been added yet.
