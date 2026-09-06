# AsDecided for Omarchy

**The human workspace for the decisions your agents follow.**

A standalone native Qt/QML application backed by the Rust engine in
[asdecided/core](https://github.com/asdecided/core). Open a local project, browse
its engineering knowledge, search with the engine, and inspect the accepted
decisions governing a code path. Markdown and Git remain the source of truth.

This is the initial read-only application milestone. It is not an Omarchy shell
plugin. Integrated authoring and federation update review are planned separately.

## What works

- Repository picker and eight recent projects; refresh after external edits.
- Artifact browsing with selectable Markdown, source identity and override history.
- Deterministic search with matched-field evidence.
- Code-path applicability with the exact matching scope declaration.
- Structural validation reports, including warnings and failures.
- Verified version-2 federation source inventory.
- Open a local artifact in your default Markdown application. Inherited sources
  remain read-only in this workflow.

No account, API key, hosted service, Python runtime or model call is needed.
The Rust backend links core at a fixed reviewed commit; no separate engine
installation is required. Parent sources must already be materialised locally.

## Build on Omarchy / Arch

Install the build tools and native dependencies:

```bash
sudo pacman -S --needed base-devel cmake rust qt6-base qt6-declarative qt6-wayland
cargo build --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ASDECIDED_BACKEND="$PWD/target/release/asdecided-desktop-backend" ./build/asdecided
```

Open a repository containing `.decided/` and `decisions/`. Legacy `.rac/` and
`rac/` projects are supported too. Custom corpus layouts and creating a new corpus
inside the app are not yet supported. Use `decided quickstart` separately when
initialising a new repository.

## Install a built bundle

CI produces an `asdecided-linux-x86_64` artifact containing a native tarball and
SHA-256 file. This is a development build, not a tagged stable release.
Qt remains a system dependency; install `qt6-base qt6-declarative qt6-wayland` on
Omarchy before launching it.

Verify the archive using its adjacent checksum file, extract it, enter the
extracted directory and run:

```bash
./install.sh
```

The installer adds **AsDecided** to your application launcher under your user
account. It retains old version directories and atomically switches the launcher
symlink on update. You can supply a different installation prefix as its first
argument. Project Markdown and application preferences are not removed by updates.

To make a bundle from a source build:

```bash
./packaging/bundle.sh
```

## Keyboard

| Shortcut | Action |
| --- | --- |
| Ctrl+O | Open project |
| Ctrl+F | Focus search / code-path input |
| Enter in input | Run search / scope lookup |
| Up / Down in list | Select artifact |
| Ctrl+R | Refresh current project |
| Ctrl+Shift+V | Validate corpus |
| Escape | Cancel current work / close report |
| Ctrl+Q | Quit |

## Development checks

```bash
cargo fmt --check
cargo clippy --locked --all-targets -- -D warnings
cargo test --locked
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ASDECIDED_BACKEND="$PWD/target/debug/asdecided-desktop-backend" \
  ./build/asdecided --smoke-test "$PWD/tests/fixture"
```

Tests exercise the real pinned engine, including a multi-parent federation,
invalid pins, path confinement and failed project selection. CI also installs and
runs the packaged application. A real Omarchy Wayland session is still required
to verify the compositor, native dialogs and default editor handoff.

See [architecture and milestone boundaries](docs/architecture.md).

Apache-2.0. The initial icon and federation fixture are reused from
`asdecided/core` at the pinned commit; see [NOTICE](NOTICE).
