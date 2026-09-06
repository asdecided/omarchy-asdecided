#!/usr/bin/env bash
# Emit a makepkg input directory from committed sources; never package the worktree.
set -euo pipefail
cd "$(dirname "$0")/../.."
revision=$(git rev-parse --verify "${1:-HEAD}^{commit}")
version=$(git show "$revision:backend/Cargo.toml" | sed -n 's/^version = "\([0-9.]*\)"$/\1/p')
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid application version' >&2; exit 1; }
count=$(git rev-list --count "$revision")
version="${version}.r${count}.g${revision:0:12}"
output="$PWD/dist/arch"
mkdir -p "$output"
archive="asdecided-${version}.tar.gz"
git archive --format=tar --prefix="asdecided-${version}/" "$revision" | gzip -n > "$output/$archive"
checksum=$(sha256sum "$output/$archive")
checksum=${checksum%% *}
git show "$revision:packaging/arch/PKGBUILD.in" | 
    sed -e "s/@VERSION@/$version/g" -e "s/@SHA256@/$checksum/g" > "$output/PKGBUILD"
printf '%s\n' "$revision" > "$output/SOURCE_COMMIT"
printf 'Prepared %s\nRun makepkg in %s\n' "$version" "$output"
