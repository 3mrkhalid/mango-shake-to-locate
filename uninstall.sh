#!/usr/bin/env bash
# Removes what install.sh created. Never touches your Mango configuration or
# any Mango installation that was not created by this installer.
set -Eeuo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=scripts/common.sh
. "$HERE/scripts/common.sh"

usage() {
	cat <<USAGE
Usage: ./uninstall.sh [options]
  --prefix DIR      install directory to remove (default: $DEFAULT_PREFIX)
  --link-dir DIR    directory holding the '$LINK_NAME' launcher (default: $DEFAULT_LINK_DIR)
  --build-root DIR  scratch directory to remove (default: $BUILD_ROOT_DEFAULT)
  --keep-cache      keep the download/build scratch directory
  --dry-run         show what would be removed
  -h, --help        show this help
USAGE
}
PREFIX="$DEFAULT_PREFIX"; LINK_DIR="$DEFAULT_LINK_DIR"; BUILD_ROOT="$BUILD_ROOT_DEFAULT"
KEEP_CACHE=0; DRY_RUN=0
while [ "$#" -gt 0 ]; do
	case "$1" in
		--prefix) [ "$#" -ge 2 ] || die "--prefix needs a value"; PREFIX="$2"; shift 2 ;;
		--link-dir) [ "$#" -ge 2 ] || die "--link-dir needs a value"; LINK_DIR="$2"; shift 2 ;;
		--build-root) [ "$#" -ge 2 ] || die "--build-root needs a value"; BUILD_ROOT="$2"; shift 2 ;;
		--keep-cache) KEEP_CACHE=1; shift ;;
		--dry-run) DRY_RUN=1; shift ;;
		-h|--help) usage; exit 0 ;;
		*) err "unknown option: $1"; usage >&2; exit 2 ;;
	esac
done

PREFIX=$(assert_safe_dir "$PREFIX")
BUILD_ROOT=$(assert_safe_dir "$BUILD_ROOT")
LINK_DIR=$(readlink -m -- "$LINK_DIR")
removed=0

link="$LINK_DIR/$LINK_NAME"
if [ -L "$link" ]; then
	case "$(readlink -- "$link")" in
		"$PREFIX"/*) info "removing launcher $link"; run rm -f -- "$link"; removed=1 ;;
		*) warn "'$link' does not point into $PREFIX; leaving it alone" ;;
	esac
fi

if [ -d "$PREFIX" ]; then
	[ -f "$PREFIX/$MARKER_NAME" ] \
		|| die "'$PREFIX' was not created by this installer (marker file missing); not removing it."
	info "removing $PREFIX"
	run rm -rf -- "$PREFIX"
	removed=1
else
	warn "nothing installed at $PREFIX"
fi

if [ "$KEEP_CACHE" = 0 ] && [ -d "$BUILD_ROOT" ]; then
	if [ -f "$BUILD_ROOT/$MARKER_NAME" ]; then
		info "removing scratch directory $BUILD_ROOT"
		run rm -rf -- "$BUILD_ROOT"
	else
		warn "'$BUILD_ROOT' was not created by this installer; leaving it alone"
	fi
fi

[ "$removed" = 1 ] && info "done. Your Mango configuration was not touched." || info "nothing to remove."
