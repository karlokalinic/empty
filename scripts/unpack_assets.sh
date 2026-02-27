#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/assets/textures"
ZIP_FILE="$ROOT_DIR/assets/bundles/textures_bundle.zip"

if ! command -v unzip >/dev/null 2>&1; then
  echo "unzip command not found. Please install unzip." >&2
  exit 1
fi

if [[ ! -f "$ZIP_FILE" ]]; then
  echo "Bundle not found: $ZIP_FILE -- generating textures directly instead."
  python3 "$ROOT_DIR/scripts/generate_textures.py"
  exit 0
fi

mkdir -p "$OUT_DIR"
unzip -o "$ZIP_FILE" -d "$OUT_DIR"

echo "Unpacked textures into: $OUT_DIR"
