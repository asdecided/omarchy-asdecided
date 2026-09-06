# Arch package

`asdecided-desktop` is a normal Pacman package. It owns `/usr/bin/asdecided`, the
adjacent Rust backend, the desktop entry, icon and licence notices. Runtime
requirements are declared in `depends`; compilers and CMake are build-only.
The executable names do not replace the core CLI, `decided`.

## Build from a reviewed checkout

On Arch, install `base-devel` and `git`, clone the repository and select a reviewed
commit. Run the following as your normal user:

```bash
./packaging/arch/prepare.sh
cd dist/arch
makepkg -si
```

The preparation script archives committed sources only, pins the source checksum,
and emits a PKGBUILD. Uncommitted edits are intentionally excluded. `makepkg`
resolves build dependencies, builds with the locked Rust core dependency, runs
the engine and Qt tests, and installs through Pacman when `-i` is supplied.

Development versions use `0.1.0.r<commit-count>.g<revision>-1`. Commit count orders
builds along the development branch; revision identifies the source. Avoid
rewriting published package history, since counts can decrease across rewrites.
A later application version sorts after these development builds. The recipe
can be regenerated with an explicit reviewed commit as `prepare.sh <commit>`.

CI runs makepkg as an unprivileged builder in Arch, then installs the package in
a separate Arch runtime without build tools. It checks Pacman ownership, launches
the actual installed application, upgrades to a second package revision and
removes it. Project and draft sentinels must survive upgrade and removal.
The second revision is a test fixture and is not included in the download.

## Existing development-script installations

The earlier installer used `~/.local/bin/asdecided` and a user desktop entry.
Those can take precedence over system files. If you installed that version,
close the app and move the old launcher symlink and
`~/.local/share/applications/io.github.asdecided.AsDecided.desktop` aside before
launching the Pacman version. Verify that the symlink points into
`~/.local/lib/asdecided/` before moving it. Custom installation prefixes require
the corresponding paths. Keep your preferences, drafts and project directories.

Pacman never deletes or takes ownership of those earlier user installations.
`command -v asdecided` should resolve to `/usr/bin/asdecided` after migration.

## Distribution boundary

A downloaded package is installed or updated using `pacman -U`; removal uses
`pacman -R`. Dependencies come from configured repositories. The adjacent SHA-256
file detects download corruption; it is not a package signing identity.

No repository entry, signing key or relaxed Pacman signature policy is installed.
Normal `pacman -Syu` discovery requires publishing to a configured package
repository. This change does not claim official Arch, AUR or Omarchy inclusion.

References: [PKGBUILD manual](https://pacman.archlinux.page/PKGBUILD.5.html) and
[Pacman manual](https://pacman.archlinux.page/pacman.8.html).
