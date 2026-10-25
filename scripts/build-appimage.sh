#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

APP_VERSION="${VERSION:-$(tr -d '\n' < VERSION)}"
ARCH="${ARCH:-$(uname -m)}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/appimage}"
APPDIR="${APPDIR:-$BUILD_DIR/AppDir}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/dist}"

LINUXDEPLOY="${LINUXDEPLOY:-$(command -v linuxdeploy || true)}"
if [[ -z "$LINUXDEPLOY" ]]; then
  echo "linuxdeploy not found in PATH" >&2
  exit 1
fi

if [[ ! -x "$LINUXDEPLOY" ]]; then
  echo "linuxdeploy is not executable: $LINUXDEPLOY" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"
rm -rf "$APPDIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR/cmake" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR/cmake" --parallel "$(nproc)"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR/cmake"

export APPIMAGE_EXTRACT_AND_RUN=1
export LINUXDEPLOY_OUTPUT_VERSION="$APP_VERSION"
rm -f "$BUILD_DIR"/*.AppImage

(
  cd "$BUILD_DIR"
  "$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/coomer.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/coomer.svg" \
    --output appimage
)

APPIMAGE_PATH="$(find "$BUILD_DIR" -maxdepth 1 -type f -name '*.AppImage' -print -quit)"
if [[ -z "$APPIMAGE_PATH" ]]; then
  echo "AppImage build completed without producing an AppImage" >&2
  exit 1
fi

mv "$APPIMAGE_PATH" "$OUTPUT_DIR/coomer-v${APP_VERSION}-${ARCH}.AppImage"
