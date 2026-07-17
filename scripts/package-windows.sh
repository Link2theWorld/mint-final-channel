#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE_BUNDLE="${1:-$ROOT/build-windows/out/VaporwaveSuite.vst3}"
PRODUCT="MINT-Final-Channel"
VERSION="0.1.0"
OUTPUT_DIR="$ROOT/dist"
STAGING="$(mktemp -d "${TMPDIR:-/tmp}/mint-final-channel-win.XXXXXX")"
PACKAGE_DIR="$STAGING/$PRODUCT-$VERSION-Windows-x64"

if [[ ! -d "$SOURCE_BUNDLE" ]]; then
  echo "Windows VST3 bundle not found: $SOURCE_BUNDLE" >&2
  exit 1
fi

trap 'rm -rf "$STAGING"' EXIT
mkdir -p "$PACKAGE_DIR" "$OUTPUT_DIR"
cp -R "$SOURCE_BUNDLE" "$PACKAGE_DIR/MINT Final Channel.vst3"
cp "$ROOT/packaging/INSTALL-WINDOWS.txt" "$PACKAGE_DIR/INSTALL-WINDOWS.txt"
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$PACKAGE_DIR/"

(
  cd "$STAGING"
  zip -q -r -X "$OUTPUT_DIR/$PRODUCT-$VERSION-Windows-x64.zip" \
    "$PRODUCT-$VERSION-Windows-x64"
)

echo "$OUTPUT_DIR/$PRODUCT-$VERSION-Windows-x64.zip"
