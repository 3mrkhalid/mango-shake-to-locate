# shellcheck shell=bash
# Shared helpers for install.sh / uninstall.sh (sourced, not executed).

MANGO_REPO_DEFAULT="https://github.com/DreamMaoMao/mangowc.git"
MANGO_COMMIT_DEFAULT="ba0c8d9859f9142d1cfcc61f39f289c545661fb9"
MARKER_NAME=".mango-shake-to-locate"
DEFAULT_PREFIX="${XDG_DATA_HOME:-$HOME/.local/share}/mango-shake-to-locate"
DEFAULT_LINK_DIR="$HOME/.local/bin"
LINK_NAME="mango-shake"
BUILD_ROOT_DEFAULT="${XDG_CACHE_HOME:-$HOME/.cache}/mango-shake-to-locate"

if [ -t 2 ]; then
	C_RED=$'\033[31m'; C_YEL=$'\033[33m'; C_GRN=$'\033[32m'; C_BLD=$'\033[1m'; C_OFF=$'\033[0m'
else
	C_RED=""; C_YEL=""; C_GRN=""; C_BLD=""; C_OFF=""
fi

info() { printf '%s==>%s %s\n' "$C_GRN" "$C_OFF" "$*" >&2; }
warn() { printf '%swarning:%s %s\n' "$C_YEL" "$C_OFF" "$*" >&2; }
err()  { printf '%serror:%s %s\n' "$C_RED" "$C_OFF" "$*" >&2; }
die()  { err "$*"; exit 1; }

# run CMD...: print it instead of running when DRY_RUN=1
run() {
	if [ "${DRY_RUN:-0}" = 1 ]; then
		printf '[dry-run] %s\n' "$*" >&2
	else
		"$@"
	fi
}

# Refuse paths that must never be deleted or used as an install prefix.
assert_safe_dir() {
	local p="$1" real
	[ -n "$p" ] || die "empty path"
	real=$(readlink -m -- "$p")
	case "$real" in
		/|/usr|/usr/*|/bin|/sbin|/lib|/lib64|/etc|/etc/*|/var|/opt|/boot|/root|/home|/tmp)
			die "refusing to use system path '$real'" ;;
	esac
	[ "$real" != "$(readlink -m -- "$HOME")" ] || die "refusing to use your home directory itself"
	printf '%s' "$real"
}
