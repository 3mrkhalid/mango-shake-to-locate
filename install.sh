#!/usr/bin/env bash
# ShakeToLocate installer for the Mango Wayland compositor.
#
# Builds a SEPARATE copy of Mango with ShakeToLocate added and installs it into
# its own directory. Your existing Mango installation and your configuration
# files are never modified.
set -Eeuo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=scripts/common.sh
. "$HERE/scripts/common.sh"

usage() {
	cat <<USAGE
Usage: ./install.sh [options]

Builds Mango (pinned commit ${MANGO_COMMIT_DEFAULT:0:7}) with ShakeToLocate and
installs it next to, not over, any existing Mango.

Options:
  --prefix DIR       install directory   (default: $DEFAULT_PREFIX)
  --link-dir DIR     where to put the '$LINK_NAME' launcher (default: $DEFAULT_LINK_DIR)
  --no-link          do not create the launcher symlink
  --mango-src DIR    use a local Mango source tree instead of cloning
                     (it is copied; the original is never changed)
  --commit SHA       Mango commit to clone (default: pinned, the only tested one)
  --repo URL         Mango git repository (default: $MANGO_REPO_DEFAULT)
  --build-root DIR   scratch directory (default: $BUILD_ROOT_DEFAULT)
  --no-xwayland      build without XWayland support
  --jobs N           parallel build jobs
  --check-only       only check dependencies, then exit
  --prepare-only     fetch + patch the source, but do not build or install
  --dry-run          print what would be done, change nothing outside the scratch dir
  -h, --help         show this help
USAGE
}

PREFIX="$DEFAULT_PREFIX"; LINK_DIR="$DEFAULT_LINK_DIR"; MAKE_LINK=1
MANGO_SRC=""; COMMIT="$MANGO_COMMIT_DEFAULT"; REPO="$MANGO_REPO_DEFAULT"
BUILD_ROOT="$BUILD_ROOT_DEFAULT"; XWAYLAND=enabled; JOBS=""
CHECK_ONLY=0; PREPARE_ONLY=0; DRY_RUN=0

need_arg() { [ "$#" -ge 2 ] && [ -n "$2" ] || die "option $1 needs a value"; }
while [ "$#" -gt 0 ]; do
	case "$1" in
		--prefix) need_arg "$@"; PREFIX="$2"; shift 2 ;;
		--link-dir) need_arg "$@"; LINK_DIR="$2"; shift 2 ;;
		--no-link) MAKE_LINK=0; shift ;;
		--mango-src) need_arg "$@"; MANGO_SRC="$2"; shift 2 ;;
		--commit) need_arg "$@"; COMMIT="$2"; shift 2 ;;
		--repo) need_arg "$@"; REPO="$2"; shift 2 ;;
		--build-root) need_arg "$@"; BUILD_ROOT="$2"; shift 2 ;;
		--no-xwayland) XWAYLAND=disabled; shift ;;
		--jobs) need_arg "$@"; JOBS="$2"; shift 2 ;;
		--check-only) CHECK_ONLY=1; shift ;;
		--prepare-only) PREPARE_ONLY=1; shift ;;
		--dry-run) DRY_RUN=1; shift ;;
		-h|--help) usage; exit 0 ;;
		*) err "unknown option: $1"; usage >&2; exit 2 ;;
	esac
done

if [ "$(id -u)" -eq 0 ] && [ "${MANGO_SHAKE_ALLOW_ROOT:-0}" != 1 ]; then
	die "do not run this as root. It installs into your home directory and needs no privileges. (Containers/CI: set MANGO_SHAKE_ALLOW_ROOT=1.)"
fi

# ------------------------------------------------------------------ dependencies
check_deps() {
	local missing_cmd=() missing_pkg=() c p
	for c in meson ninja pkg-config patch cc; do
		command -v "$c" >/dev/null 2>&1 || missing_cmd+=("$c")
	done
	[ -n "$MANGO_SRC" ] || command -v git >/dev/null 2>&1 || missing_cmd+=("git")
	command -v wayland-scanner >/dev/null 2>&1 || missing_cmd+=("wayland-scanner")

	if command -v pkg-config >/dev/null 2>&1; then
		# module names and minimum versions taken from Mango's meson.build
		local specs=(
			"wayland-server >= 1.23.1" "wayland-client" "wayland-protocols"
			"wlroots-0.20 >= 0.20.0" "scenefx-0.5 >= 0.5.0"
			"libdrm" "xkbcommon" "libinput >= 1.27.1" "pixman-1"
			"libpcre2-8" "libcjson" "pangocairo"
		)
		if [ "$XWAYLAND" = enabled ]; then
			specs+=("xcb" "xcb-icccm" "xcb-randr")
		fi
		for p in "${specs[@]}"; do
			pkg-config --exists "$p" 2>/dev/null || missing_pkg+=("$p")
		done
	fi

	if [ "${#missing_cmd[@]}" -eq 0 ] && [ "${#missing_pkg[@]}" -eq 0 ]; then
		info "all build dependencies found"
		return 0
	fi

	err "missing build requirements:"
	[ "${#missing_cmd[@]}" -eq 0 ] || printf '  programs:    %s\n' "${missing_cmd[*]}" >&2
	if [ "${#missing_pkg[@]}" -gt 0 ]; then
		printf '  pkg-config modules:\n' >&2
		printf '    - %s\n' "${missing_pkg[@]}" >&2
	fi
	cat >&2 <<HELP

Install the development packages that provide them with your distribution's
package manager. Mango also needs wlroots and scenefx in the exact versions
above; many distributions do not ship them yet, in which case they must be built
from source. Mango's own instructions are the reference:
  https://github.com/mangowm/mango  (docs/installation.md, "Building from Source")
Note: if your Mango comes from a distribution package, this build still needs
the *development* files for the libraries listed above.
Without XWayland support you can skip the xcb modules: --no-xwayland
HELP
	return 1
}

if [ "$PREPARE_ONLY" = 1 ] && [ "$CHECK_ONLY" = 0 ]; then
	info "--prepare-only: skipping the build dependency check"
elif check_deps; then
	[ "$CHECK_ONLY" = 0 ] || exit 0
else
	[ "$CHECK_ONLY" = 0 ] || exit 1
	if [ "$DRY_RUN" = 1 ]; then warn "(dry run: continuing anyway)"; else exit 1; fi
fi

# ------------------------------------------------------------------ paths
PREFIX=$(assert_safe_dir "$PREFIX")
BUILD_ROOT=$(assert_safe_dir "$BUILD_ROOT")
[ "$MAKE_LINK" = 0 ] || LINK_DIR=$(readlink -m -- "$LINK_DIR")
SRC="$BUILD_ROOT/mango"
BUILD="$BUILD_ROOT/build"

for f in "$HERE"/patches/0001-*.patch "$HERE/src/input/shake_to_locate.c" \
         "$HERE/include/mango/input/shake_to_locate.h"; do
	[ -f "$f" ] || die "incomplete download: missing $f"
done
PATCH_FILE=$(ls "$HERE"/patches/0001-*.patch)

if [ -e "$PREFIX" ] && [ ! -f "$PREFIX/$MARKER_NAME" ] && [ -n "$(ls -A "$PREFIX" 2>/dev/null)" ]; then
	die "'$PREFIX' exists and was not created by this installer. Choose another --prefix."
fi

# ------------------------------------------------------------------ source
info "preparing Mango source in $SRC"
if [ -d "$BUILD_ROOT" ] && [ ! -f "$BUILD_ROOT/$MARKER_NAME" ] && [ -n "$(ls -A "$BUILD_ROOT" 2>/dev/null)" ]; then
	die "'$BUILD_ROOT' exists and was not created by this installer. Choose another --build-root."
fi
mkdir -p "$BUILD_ROOT"; : > "$BUILD_ROOT/$MARKER_NAME"
rm -rf "$SRC" "$BUILD"

if [ -n "$MANGO_SRC" ]; then
	[ -f "$MANGO_SRC/meson.build" ] && grep -q "project('mango'" "$MANGO_SRC/meson.build" \
		|| die "'$MANGO_SRC' does not look like a Mango source tree (no project('mango') in meson.build)"
	cp -a -- "$MANGO_SRC" "$SRC"
	warn "using your local tree; only the pinned commit ${MANGO_COMMIT_DEFAULT:0:7} is known to work"
else
	git init -q "$SRC"
	git -C "$SRC" remote add origin "$REPO"
	git -C "$SRC" fetch -q --depth 1 origin "$COMMIT" \
		|| die "could not fetch commit $COMMIT from $REPO (network problem or wrong commit)"
	git -C "$SRC" checkout -q FETCH_HEAD
	[ "$COMMIT" = "$MANGO_COMMIT_DEFAULT" ] \
		|| warn "commit differs from the tested one (${MANGO_COMMIT_DEFAULT:0:7}); the patch may not apply"
fi

if grep -rqs "shake_to_locate" "$SRC/src" "$SRC/include"; then
	die "this Mango source already contains ShakeToLocate; nothing to do"
fi

info "applying integration patch"
if ! patch -p1 -d "$SRC" --dry-run -s < "$PATCH_FILE" >/dev/null; then
	err "the integration patch does not apply to this Mango source."
	err "This installer supports Mango commit ${MANGO_COMMIT_DEFAULT:0:7} (0.18.5 + 15 commits)."
	err "Use that commit (the default) or open an issue with your Mango version."
	exit 1
fi
patch -p1 -d "$SRC" -s < "$PATCH_FILE"
mkdir -p "$SRC/src/input" "$SRC/include/mango/input"
cp -- "$HERE/src/input/shake_to_locate.c" "$SRC/src/input/"
cp -- "$HERE/include/mango/input/shake_to_locate.h" "$SRC/include/mango/input/"
info "source ready: $SRC"

if [ "$PREPARE_ONLY" = 1 ]; then
	info "--prepare-only: stopping before build"
	exit 0
fi

# ------------------------------------------------------------------ build
info "configuring"
run meson setup "$BUILD" "$SRC" --prefix "$PREFIX" --buildtype=release -Dxwayland="$XWAYLAND" \
	|| die "meson setup failed (see the messages above, usually a missing dependency)"
info "building"
run ninja -C "$BUILD" ${JOBS:+-j "$JOBS"} || die "build failed"

# ------------------------------------------------------------------ install
info "installing into $PREFIX"
run mkdir -p "$PREFIX"
run meson install -C "$BUILD" --quiet || die "install failed"
if [ "$DRY_RUN" != 1 ]; then
	{
		echo "installer=mango-shake-to-locate"
		echo "mango_commit=$COMMIT"
		echo "installed=$(date -u +%FT%TZ)"
		echo "link=$([ "$MAKE_LINK" = 1 ] && echo "$LINK_DIR/$LINK_NAME" || true)"
	} > "$PREFIX/$MARKER_NAME"
	mkdir -p "$PREFIX/share/mango-shake-to-locate"
	cp -- "$HERE/config/shake-to-locate.conf.example" "$PREFIX/share/mango-shake-to-locate/"
fi

if [ "$MAKE_LINK" = 1 ]; then
	target="$PREFIX/bin/mango"
	link="$LINK_DIR/$LINK_NAME"
	run mkdir -p "$LINK_DIR"
	if [ -L "$link" ] && [ "$(readlink -- "$link")" = "$target" ]; then
		:
	elif [ -e "$link" ] || [ -L "$link" ]; then
		warn "'$link' already exists and is not ours; not touching it. Run $target directly."
	else
		run ln -s -- "$target" "$link"
	fi
fi

cat <<DONE

${C_BLD}ShakeToLocate installed.${C_OFF}  Your existing Mango and configuration were not changed.

Start the new build from a TTY (log out of your current Mango session first):
  $([ "$MAKE_LINK" = 1 ] && echo "$LINK_NAME" || echo "$PREFIX/bin/mango")

ShakeToLocate is ON by default. To tune it, add the lines from
  $PREFIX/share/mango-shake-to-locate/shake-to-locate.conf.example
to your Mango config (~/.config/mango/config.conf), then reload Mango.
This installer never edits that file.

To make your display manager start it, point a session entry at:
  $PREFIX/bin/mango
Remove everything again with:  ./uninstall.sh
DONE
