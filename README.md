# AsDecided for Omarchy

**The human workspace for the decisions your agents follow.**

A standalone native Qt/QML application backed by the Rust engine in
[asdecided/core](https://github.com/asdecided/core). Open a local project, browse
its engineering knowledge, search with the engine, and inspect the accepted
decisions governing a code path. Markdown and Git remain the source of truth.

A dark, keyboard-first knowledge workspace with a file explorer, document tabs,
split Markdown editing and preview, and a linked decision-context inspector.
This is a development application; the first real Omarchy acceptance test is pending.

## What works

- Open or initialize a project, with eight recent projects.
- Collapsible artifact explorer and multiple document tabs.
- Source, split and reading views with Markdown syntax highlighting and zoom.
- Core templates for decisions, requirements, designs, roadmaps and prompts.
- Reviewed saves: core validation, readable findings, diff and explicit apply.
- Atomic file replacement, external-edit conflict detection and local draft recovery.
- Outgoing relationships, backlinks, source identity and override history.
- Engine search with matched-field evidence, and code-path applicability.
- Corpus validation, federation source inventory and external editor handoff.
- Command palette and unsaved-work prompts. Inherited artifacts stay read-only.

No account, API key, hosted service, Python runtime or model call is needed.
The Rust backend links core at a fixed reviewed commit; no separate engine
installation is required. Parent sources must already be materialised locally.

## Install on Omarchy / Arch

Open the [public development releases](https://github.com/asdecided/omarchy-asdecided/releases)
and copy the **Download, verify and install with Pacman** commands from the release notes.
They download and verify the package from GitHub, then install it with Pacman; no account, ZIP
extraction or local compiler is needed.

Alternatively, download the `.pkg.tar.zst` and adjacent checksum from the release,
then install the local file:

```bash
sha256sum -c asdecided-desktop-*.pkg.tar.zst.sha256
sudo pacman -U ./asdecided-desktop-*.pkg.tar.zst
```

Pacman installs the native app, backend and launcher system-wide and resolves Qt
and other runtime dependencies from your configured Arch repositories. Open
**AsDecided** from your application launcher. No local compiler is needed.

To update, use the newer release's `pacman -U` command. To remove the application:

```bash
sudo pacman -R asdecided-desktop
```

Your project Markdown, preferences and recovered drafts remain in your home
folder. The package contains no scripts that modify user files.

This is a development package. It is not yet published in Arch's repositories,
the AUR or an Omarchy package repository; `pacman -S asdecided-desktop` and automatic
`pacman -Syu` updates are not available. A configured repository is the next
separate distribution step. See [Arch packaging](docs/arch-packaging.md).

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
`rac/` projects are supported too. Choose **Initialize project** to create a new
corpus in an existing folder. Custom corpus layouts are not supported.

## Legacy development bundle

For development outside Arch, CI also produces an `asdecided-linux-x86_64` artifact containing a native tarball and
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
| Ctrl+N | New artifact from a core template |
| Ctrl+S | Validate and review changes before saving |
| Ctrl+P | Commands |
| Ctrl+E | Cycle source / split / read |
| Ctrl+Tab | Next document tab |
| Ctrl+W | Close document tab |
| Ctrl+= / Ctrl+- | Editor and preview zoom |
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

Tests exercise the pinned engine, multi-parent federation, invalid pins, path
confinement, template creation, validation, save conflicts and recovery. A Qt test
loads the actual QML editor, changes text, reviews and saves to a temporary project.
CI also installs and runs the packaged application. A real Omarchy Wayland session is still required
to verify the compositor, native dialogs and default editor handoff.

## First laptop test

1. Install the latest PR artifact and launch **AsDecided** from the app launcher.
2. Open a disposable copy of a project; browse and search its decisions.
3. Edit a local document, switch tabs, return, then press Ctrl+S to review and apply.
4. Create a new artifact, complete its template sections, and review its first save.
5. Edit an open file externally and confirm the app preserves your draft on conflict.
6. Check window resizing, keyboard focus, folder dialogs and external file handoff.

Draft recovery is local application data, not a repository commit. Explicit discard
removes the recovered buffer. Saves change Markdown files; Git commits remain yours.
The app does not yet rename/delete artifacts, fetch or repin federation sources,
or perform multi-file supersession transactions. Relationships can be edited in
Markdown and are checked by core before save. Preview links and embedded images
are not activated; use the relationship inspector to navigate recorded links.

See [architecture](docs/architecture.md) and the [authoring contract](docs/authoring.md).

Apache-2.0. The initial icon and federation fixture are reused from
`asdecided/core` at the pinned commit; see [NOTICE](NOTICE).
