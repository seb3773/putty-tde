#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SRC_ROOT/build"

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "Missing tool: $1" 1>&2
		return 1
	fi
	return 0
}

need_cmd cmake
need_cmd pkg-config

mkdir -p -- "$BUILD_DIR"

echo "info: building PuTTY-TDE in Release mode with aggressive optimizations..."
cmake -S "$SRC_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DPUTTY_AGGRESSIVE_FLAGS=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"

BIN_PATH="$BUILD_DIR/putty-tde"
if test -x "$BIN_PATH"; then
	if command -v sstrip >/dev/null 2>&1; then
		echo "info: stripping binary with sstrip"
		sstrip "$BIN_PATH" >/dev/null 2>&1 || true
	else
		strip --strip-all "$BIN_PATH" >/dev/null 2>&1 || true
	fi
	echo "ok: $BIN_PATH ($(stat -c%s "$BIN_PATH") bytes)"
else
	echo "FAIL: $BIN_PATH not found" 1>&2
	exit 1
fi
