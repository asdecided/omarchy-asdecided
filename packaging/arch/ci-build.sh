#!/usr/bin/env bash
# Invoked as root inside the disposable Arch build container, never on a host.
set -euo pipefail
pacman -Syu --noconfirm --needed cmake rust git qt6-base qt6-declarative qt6-wayland xdg-utils desktop-file-utils
useradd --create-home builder
chown -R builder:builder /work
runuser -u builder -- bash -euo pipefail <<'BUILD'
cd /work
./packaging/arch/prepare.sh
cd dist/arch
makepkg --noconfirm
makepkg --printsrcinfo > .SRCINFO
package=$(makepkg --packagelist)
[[ $(printf '%s\n' "$package" | wc -l) == 1 && -f $package ]]
sha256sum "$(basename "$package")" > "$(basename "$package").sha256"
# A second package revision exercises a genuine upgrade using the same binaries.
mkdir upgrade-test
sed -i 's/^pkgrel=1$/pkgrel=2/' PKGBUILD
PKGDEST="$PWD/upgrade-test" makepkg --repackage --noconfirm
sed -i 's/^pkgrel=2$/pkgrel=1/' PKGBUILD
BUILD
