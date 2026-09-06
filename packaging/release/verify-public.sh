#!/usr/bin/env bash
# Disposable Arch runtime: authenticate no downloads, retain normal Pacman policy.
set -euo pipefail
: "${PACKAGE_URL:?}" "${CHECKSUM_URL:?}" "${EXPECTED_REVISION:?}"
[[ $PACKAGE_URL == https://github.com/asdecided/omarchy-asdecided/releases/download/dev-"$EXPECTED_REVISION"/* ]]
[[ $CHECKSUM_URL == "$PACKAGE_URL.sha256" ]]
pacman-key --init
pacman -Syu --noconfirm --needed curl
mkdir -p /tmp/public-package
cd /tmp/public-package
curl --fail --location --retry 3 --output package.sha256 "$CHECKSUM_URL"
package=${PACKAGE_URL##*/}
[[ $package =~ ^asdecided-desktop-[a-zA-Z0-9._-]+\.pkg\.tar\.zst$ ]]
curl --fail --location --retry 3 --output "$package" "$PACKAGE_URL"
sha256sum -c package.sha256
# Install the verified download using the standard local-file policy.
pacman -U --noconfirm "./$package"
[[ $(pacman -Q asdecided-desktop) == *"g${EXPECTED_REVISION:0:12}"* ]]
pacman -Qkk asdecided-desktop
[[ $(stat -c '%u:%g' /usr/bin/asdecided) == 0:0 ]]
useradd --create-home tester
runuser -u tester -- env -u ASDECIDED_BACKEND \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    /usr/bin/asdecided --smoke-test /fixture 2> /tmp/public-qml.log
cat /tmp/public-qml.log
! grep -E 'qrc:|failed to load component' /tmp/public-qml.log
pacman -R --noconfirm asdecided-desktop
[[ ! -e /usr/bin/asdecided ]]
