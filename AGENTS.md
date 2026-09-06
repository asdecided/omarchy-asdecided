# AsDecided native companion

- This repository owns a standalone Qt/QML application for Omarchy, not a shell plugin.
- Rust owns project access and engine integration. Keep C++ limited to Qt lifecycle,
  process transport, presentation and desktop integration. Do not add a Python runtime.
- Reuse the pinned `asdecided/core` implementation for classification, search,
  validation, scope and federation. Do not fork these semantics into the app.
- Corpus Markdown and repository configuration are read-only in this milestone.
  Any future writes require a reviewed design, diff preview and explicit user action.
- Preserve source identity, inherited read-only boundaries and override history.
  Do not imply search or scope results prove actual agent consumption.
- Requests go through bounded stdin to the sibling backend, not a shell or a daemon.
- Run `cargo fmt --check`, `cargo clippy --locked --all-targets -- -D warnings`,
  `cargo test --locked`, the CMake build, CTest and the QML smoke test.
- Keep release packaging and the installed-app smoke test working. Document any
  native Wayland behaviour that was not verified on an actual Omarchy desktop.
