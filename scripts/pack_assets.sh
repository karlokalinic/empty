#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT_DIR/assets/textures"
OUT_DIR="$ROOT_DIR/assets/bundles"
OUT_FILE="$OUT_DIR/textures_bundle.zip"

mkdir -p "$OUT_DIR"

python3 "$ROOT_DIR/scripts/generate_textures.py"

if ! command -v zip >/dev/null 2>&1; then
  echo "zip command not found. Please install zip." >&2
  exit 1
fi

zip -j -9 "$OUT_FILE" "$SRC_DIR"/*.bmp

echo "Packed textures to: $OUT_FILE"
