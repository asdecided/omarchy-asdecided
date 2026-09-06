#!/usr/bin/env bash
# Run after both native and Arch CI jobs succeed. GH_TOKEN is supplied by Actions.
set -euo pipefail
: "${GITHUB_REPOSITORY:?}" "${GITHUB_SHA:?}"
[[ $GITHUB_REPOSITORY == asdecided/omarchy-asdecided && $GITHUB_SHA =~ ^[0-9a-f]{40}$ ]]
cd "${1:?artifact directory}"
[[ $(cat SOURCE_COMMIT) == "$GITHUB_SHA" ]]
shopt -s nullglob
packages=(asdecided-desktop-*.pkg.tar.zst)
[[ ${#packages[@]} == 1 ]]
package=${packages[0]}
[[ $package =~ ^[a-zA-Z0-9._-]+$ ]]
sha256sum -c "$package.sha256"
tag="dev-$GITHUB_SHA"
base="https://github.com/$GITHUB_REPOSITORY/releases/download/$tag"
notes=$(mktemp)
verify_dir=$(mktemp -d)
trap 'rm -f "$notes"; rm -rf "$verify_dir"' EXIT
cat > "$notes" <<NOTES
Native AsDecided development build for Omarchy / Arch Linux (Intel/AMD 64-bit).

Built from $GITHUB_SHA. Native tests and Arch install, upgrade and removal checks passed before publication. Real Omarchy/Wayland desktop acceptance remains pending.

Download, verify and install with Pacman:

\`\`\`bash
curl -fLO $base/$package && \
  curl -fLO $base/$package.sha256 && \
  sha256sum -c $package.sha256 && \
  sudo pacman -U ./$package
\`\`\`

Open **AsDecided** from the application launcher. Pacman resolves the Qt dependencies. Updates use the newer release's install command; remove with \`sudo pacman -R asdecided-desktop\`. Project files and drafts are preserved.

These are development packages, not a stable release or a configured package repository. The checksum is provided for download integrity; no signing-policy changes are required. An old user-local installation may shadow the system launcher; see the README's migration guidance.
NOTES
if ! gh release view "$tag" --repo "$GITHUB_REPOSITORY" >/dev/null 2>&1; then
    gh release create "$tag" --repo "$GITHUB_REPOSITORY" --target "$GITHUB_SHA" \
        --draft --prerelease --title "AsDecided development ${GITHUB_SHA:0:12}" --notes-file "$notes"
fi
[[ $(gh release view "$tag" --repo "$GITHUB_REPOSITORY" --json targetCommitish --jq .targetCommitish) == "$GITHUB_SHA" ]]
# A retry may complete a partially uploaded draft, but never replace existing bytes.
assets=$(gh release view "$tag" --repo "$GITHUB_REPOSITORY" --json assets --jq '.assets[].name')
for file in "$package" "$package.sha256" PKGBUILD .SRCINFO SOURCE_COMMIT; do
    if grep -Fxq -- "$file" <<< "$assets"; then
        gh release download "$tag" --repo "$GITHUB_REPOSITORY" --pattern "$file" --dir "$verify_dir"
        cmp -- "$file" "$verify_dir/$file"
    else
        gh release upload "$tag" "$file" --repo "$GITHUB_REPOSITORY"
    fi
done
gh release edit "$tag" --repo "$GITHUB_REPOSITORY" --draft=false --prerelease --latest=false
[[ $(gh api "repos/$GITHUB_REPOSITORY/commits/$tag" --jq .sha) == "$GITHUB_SHA" ]]
printf 'package_url=%s/%s\nchecksum_url=%s/%s.sha256\n' "$base" "$package" "$base" "$package" >> "${GITHUB_OUTPUT:?}"
