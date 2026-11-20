#!/usr/bin/env bash
# Build Tauri GUI apps (viewer + archiver) in release mode.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
export http_proxy="${http_proxy:-http://localhost:8118}"
export https_proxy="${https_proxy:-http://localhost:8118}"
export HTTP_PROXY="${HTTP_PROXY:-$http_proxy}"
export HTTPS_PROXY="${HTTPS_PROXY:-$https_proxy}"
# Keep artifacts inside each app tree (ignore user CARGO_TARGET_DIR).
unset CARGO_TARGET_DIR

for app in dwarfs-viewer dwarfs-archiver; do
  dir="$root/apps/$app"
  echo "==> Building $app"
  (cd "$dir" && npm install && npm run build)
  (cd "$dir/src-tauri" && cargo build --release --target-dir "$dir/src-tauri/target")
done

echo "Binaries:"
ls -la "$root/apps/dwarfs-viewer/src-tauri/target/release/dwarfs-viewer" \
       "$root/apps/dwarfs-archiver/src-tauri/target/release/dwarfs-archiver"
