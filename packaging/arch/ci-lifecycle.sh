#!/usr/bin/env bash
# Run in a fresh archlinux:base container with only the package and fixtures mounted.
set -euo pipefail
pacman-key --init
pacman -Syu --noconfirm
shopt -s nullglob
packages=(/packages/*.pkg.tar.zst)
upgrades=(/packages/upgrade-test/*.pkg.tar.zst)
[[ ${#packages[@]} == 1 && ${#upgrades[@]} == 1 ]]
(cd /packages && sha256sum -c "$(basename "${packages[0]}").sha256")
pacman -U --noconfirm "${packages[0]}"
first_version=$(pacman -Q asdecided-desktop)
for path in /usr/bin/asdecided /usr/bin/asdecided-desktop-backend \
    /usr/share/applications/io.github.asdecided.AsDecided.desktop \
    /usr/share/icons/hicolor/64x64/apps/io.github.asdecided.AsDecided.png \
    /usr/share/licenses/asdecided-desktop/LICENSE; do
    [[ $(pacman -Qoq "$path") == asdecided-desktop ]]
done
pacman -Qkk asdecided-desktop
# Check Unix ownership as well as Pacman's file inventory: fakeroot must never
# leak the build user's UID into system files.
while IFS= read -r path; do
    [[ $(stat -c '%u:%g' "$path") == 0:0 ]]
done < <(pacman -Qlq asdecided-desktop)
useradd --create-home tester
runuser -u tester -- mkdir -p /home/tester/projects /home/tester/.local/share/AsDecided/AsDecided/drafts
runuser -u tester -- bash -c 'printf "Keep my project\n" > ~/projects/decision.md; printf "Keep my draft\n" > ~/.local/share/AsDecided/AsDecided/drafts/test.json'
smoke() {
    runuser -u tester -- env -u ASDECIDED_BACKEND \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
        /usr/bin/asdecided --smoke-test /fixture 2> /tmp/asdecided-package.log
    cat /tmp/asdecided-package.log
    ! grep -E 'qrc:|failed to load component' /tmp/asdecided-package.log
}
smoke
pacman -U --noconfirm "${upgrades[0]}"
[[ $(pacman -Q asdecided-desktop) != "$first_version" ]]
pacman -Qkk asdecided-desktop
smoke
pacman -R --noconfirm asdecided-desktop
! pacman -Q asdecided-desktop
[[ ! -e /usr/bin/asdecided && ! -e /usr/bin/asdecided-desktop-backend ]]
[[ ! -e /usr/share/applications/io.github.asdecided.AsDecided.desktop ]]
grep -Fx 'Keep my project' /home/tester/projects/decision.md
grep -Fx 'Keep my draft' /home/tester/.local/share/AsDecided/AsDecided/drafts/test.json
