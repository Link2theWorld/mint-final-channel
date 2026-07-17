#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE_BUNDLE="${1:-$ROOT/build-universal/out/VaporwaveSuite.vst3}"
PRODUCT="Mint制作-调音的终结和最后的通道条"
VERSION="0.1.0"
OUTPUT_DIR="$ROOT/dist"
STAGING="$(mktemp -d "${TMPDIR:-/tmp}/mint-final-channel.XXXXXX")"
PACKAGE_DIR="$STAGING/$PRODUCT-$VERSION-macOS-Universal"

if [[ ! -d "$SOURCE_BUNDLE" ]]; then
  echo "VST3 bundle not found: $SOURCE_BUNDLE" >&2
  exit 1
fi

mkdir -p "$PACKAGE_DIR" "$OUTPUT_DIR"
ditto "$SOURCE_BUNDLE" "$PACKAGE_DIR/$PRODUCT.vst3"
cp "$ROOT/packaging/安装说明.txt" "$PACKAGE_DIR/"
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$PACKAGE_DIR/"
ditto -c -k --sequesterRsrc --keepParent \
  "$PACKAGE_DIR" "$OUTPUT_DIR/$PRODUCT-$VERSION-macOS-Universal.zip"

echo "$OUTPUT_DIR/$PRODUCT-$VERSION-macOS-Universal.zip"

