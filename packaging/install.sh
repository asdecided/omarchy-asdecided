#!/usr/bin/env bash
# Run from an extracted native bundle. Qt 6 comes from the system package manager.
set -euo pipefail
bundle_dir=$(cd -- "$(dirname -- "$0")" && pwd)
prefix=${1:-"$HOME/.local"}
mkdir -p -- "$prefix"
prefix=$(cd -- "$prefix" && pwd)
read -r build_id < "$bundle_dir/BUILD_ID"
[[ $build_id =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid bundle build ID' >&2; exit 1; }
for name in asdecided asdecided-desktop-backend; do
    [[ -x "$bundle_dir/$name" ]] || { echo "Missing executable: $name" >&2; exit 1; }
done
"$bundle_dir/asdecided-desktop-backend" --version
releases="$prefix/lib/asdecided"
destination="$releases/$build_id"
mkdir -p -- "$releases" "$prefix/bin" "$prefix/share/applications" "$prefix/share/icons/hicolor/64x64/apps"
if [[ ! -d $destination ]]; then
    staging=$(mktemp -d "$releases/.install.XXXXXX")
    trap 'rm -rf -- "$staging"' EXIT
    install -m755 "$bundle_dir/asdecided" "$bundle_dir/asdecided-desktop-backend" "$staging/"
    install -m644 "$bundle_dir/LICENSE" "$bundle_dir/NOTICE" "$staging/"
    mv -- "$staging" "$destination"
    trap - EXIT
else
    cmp -- "$bundle_dir/asdecided" "$destination/asdecided"
    cmp -- "$bundle_dir/asdecided-desktop-backend" "$destination/asdecided-desktop-backend"
fi
launcher="$prefix/bin/asdecided"
if [[ -e $launcher || -L $launcher ]]; then
    [[ -L $launcher && $(readlink -- "$launcher") == "$releases/"* ]] || { echo "Refusing to replace unrelated launcher: $launcher" >&2; exit 1; }
fi
# The only activation change is an atomic launcher swap; old versions are retained.
link_dir=$(mktemp -d "$prefix/bin/.asdecided.XXXXXX")
trap 'rm -rf -- "$link_dir"' EXIT
ln -s -- "$destination/asdecided" "$link_dir/asdecided"
mv -Tf -- "$link_dir/asdecided" "$launcher"
rmdir -- "$link_dir"
trap - EXIT
# Desktop Entry quoting is not shell quoting. Escape reserved characters in Exec.
escaped=${launcher//\\/\\\\}
escaped=${escaped//\"/\\\"}
escaped=${escaped//\$/\\\$}
escaped=${escaped//\`/\\\`}
escaped=${escaped//%/%%}
desktop="$prefix/share/applications/io.github.asdecided.AsDecided.desktop"
while IFS= read -r line; do
    if [[ $line == Exec=* ]]; then printf 'Exec="%s" %%f\n' "$escaped"; else printf '%s\n' "$line"; fi
done < "$bundle_dir/io.github.asdecided.AsDecided.desktop" > "$desktop"
install -m644 "$bundle_dir/io.github.asdecided.AsDecided.png" "$prefix/share/icons/hicolor/64x64/apps/io.github.asdecided.AsDecided.png"
if command -v update-desktop-database >/dev/null; then update-desktop-database "$prefix/share/applications"; fi
printf 'Installed AsDecided. Open it from your application launcher or run:\n%s\n' "$launcher"
