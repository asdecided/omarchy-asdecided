# Native companion architecture

Status: local authoring development milestone, 6 September 2026.

The application is named **AsDecided**. The repository is
`asdecided/omarchy-asdecided`. Omarchy is the first target desktop. This is a
standalone window launched through a Desktop Entry; no shell-plugin manifest,
Quickshell host, browser runtime or resident service is involved.

## Ownership

- Qt Quick Controls/QML: keyboard-first explorer, tabs, source editor, review dialogs and inspector.
- Small C++ adapter: Qt process lifecycle, bounded transport, view-model mapping,
  native Markdown rendering, draft recovery, file handoff and recent-project preferences.
- Rust backend: explicit repository selection, corpus access, editor-path checks
  and explicit core-backed read, review and write operations.
- `asdecided/core`: all classification, parsing, search/ranking, validation,
  applicability, source composition, pin verification and override semantics.

Core is statically linked at commit
`d44a4d739527b9c975d8dcee7fb26be7aae4897f` (reports 0.29.0). The backend cannot
accidentally execute a different `decided` found on PATH. Cargo.lock pins the
transitive dependency set. Future engine changes belong in core, followed by an
explicit dependency update here.

## Contract and lifecycle

The window starts its sibling `asdecided-desktop-backend` executable once per
operation, sends one UTF-8 JSON request on stdin, then closes stdin. Protocol 1:

```json
{"protocol":1,"operation":"open","project":"/absolute/repository","query":""}
```

Supported operations: `open`, `search`, `scope`, `validate`, `federation`, `locate`,
`read`, `template`, `review`, `save`, `graph`, `initialize`.
There is no arbitrary CLI passthrough. Application operations return protocol-1 envelopes; engine reports retain core's
unmodified schema-version-1 JSON.
Validation exit 1 carries findings; other nonzero exits are operational failures.
Malformed JSON and unsupported response versions are refused.

Requests are bounded to 8 MiB; individual edited documents to 1 MiB. The window caps stdout at 32 MiB and stderr at
64 KiB, sets a 30-second deadline, and kills/reaps the worker on cancellation or
shutdown. It permits one operation at a time, so an old result cannot overwrite a
newer project. A failed project selection leaves the prior project intact.
No background process remains between requests. Core may use its own local cache
and existing consent-gated local usage recorder; application code adds no telemetry.

The test-only/developer `ASDECIDED_BACKEND` environment override is explicit; the
normal packaged app resolves the adjacent backend by absolute path.

## Corpus and trust boundaries

Choose the repository root containing `decisions/` and `.decided/`, or the legacy
`rac/` and `.rac/` layout. Custom corpus directories are not supported in this
milestone. Selection never silently climbs into an ancestor repository.

The browser consumes core's document export and keeps source, layer, pins and the
complete override history. It includes historical inherited records; the UI warns
when override history is present. Search and code-path scope are computed by core,
with source-aware result mapping. A match that cannot be joined to the displayed
snapshot requires a refresh. Results are snapshots; refresh after external edits.

Source is selectable, editable UTF-8 Markdown. Qt renders a separate local preview
with HTML disabled and resource loading blocked. The explicit Open file action accepts
only current local export members, resolves the real path inside the local corpus,
and rechecks inherited materialisation boundaries before desktop handoff. There
is a normal filesystem race between verification and an external editor opening a
path; this is a desktop convenience, not a filesystem sandbox.

Federation status supports core's version-2 manifests. An absent/version-1 manifest
produces an explanatory engine diagnostic. Browsing retains core's version-1 and
version-2 support. No source fetches, updates or repins are added. Explicit local corpus writes follow
the [authoring contract](authoring.md).

## Current boundaries

The workspace supports local template creation and single-document editing,
proposed-content validation, diff review, atomic saves and conflict detection.
Draft recovery stores unsaved sessions outside the repository with owner-only
permissions, retaining the original fingerprint for conflict checks on recovery.

Federation updates, renames, deletions and multi-file supersession need separate
review semantics. Git commits remain external. Relationship navigation reflects
the engine's on-disk graph and refreshes after save. It is not a draft graph or an
agent-consumption record.

The next acceptance gate is installation and keyboard use on a real Omarchy
Wayland session. Headless Qt rendering and bridge tests do not prove compositor,
file-association, launcher or platform-dialog behaviour there.
