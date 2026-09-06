#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
revision=$(git rev-parse --short=12 HEAD)
build_id="0.1.0-${revision}-$(uname -m)"
bundle="dist/asdecided-${build_id}"
mkdir -p "$bundle"
install -m755 build/asdecided target/release/asdecided-desktop-backend "$bundle/"
install -m755 packaging/install.sh "$bundle/install.sh"
install -m644 packaging/io.github.asdecided.AsDecided.desktop packaging/io.github.asdecided.AsDecided.png LICENSE NOTICE "$bundle/"
printf '%s\n' "$build_id" > "$bundle/BUILD_ID"
tar -C dist -czf "${bundle}.tar.gz" "$(basename "$bundle")"
(cd dist && sha256sum "$(basename "$bundle").tar.gz" > "$(basename "$bundle").tar.gz.sha256")
printf 'Bundle: %s.tar.gz\n' "$bundle"
