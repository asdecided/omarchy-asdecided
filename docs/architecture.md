# Native companion architecture

Status: initial read-only application milestone, 6 September 2026.

The application is named **AsDecided**. The repository is
`asdecided/omarchy-asdecided`. Omarchy is the first target desktop. This is a
standalone window launched through a Desktop Entry; no shell-plugin manifest,
Quickshell host, browser runtime or resident service is involved.

## Ownership

- Qt Quick Controls/QML: keyboard-first project, list and reading surfaces.
- Small C++ adapter: Qt process lifecycle, bounded transport, view-model mapping,
  native file handoff and recent-project UI preferences.
- Rust backend: explicit repository selection, corpus access, editor-path checks
  and a curated read-only engine command surface.
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

Supported operations: `open`, `search`, `scope`, `validate`, `federation`, `locate`.
There is no arbitrary CLI passthrough. Open and locate return application protocol
1 envelopes. Other operations return the core's unmodified schema-version-1 JSON.
Validation exit 1 carries findings; other nonzero exits are operational failures.
Malformed JSON and unsupported response versions are refused.

Requests are bounded to 64 KiB. The window caps stdout at 32 MiB and stderr at
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

Document text is selectable plain Markdown. It is deliberately not interpreted as
HTML, remote images or executable links. The explicit Open file action accepts
only current local export members, resolves the real path inside the local corpus,
and rechecks inherited materialisation boundaries before desktop handoff. There
is a normal filesystem race between verification and an external editor opening a
path; this is a desktop convenience, not a filesystem sandbox.

Federation status supports core's version-2 manifests. An absent/version-1 manifest
produces an explanatory engine diagnostic. Browsing retains core's version-1 and
version-2 support. No source fetches, updates, repins or corpus writes are added.

## Milestones

1. **This change:** launch, open/reopen project, browse, engine search, code-path
   applicability, validation, source status and external editor handoff.
2. **Authoring:** core-backed creation templates, amend/supersede workflow,
   proposed-content validation, diff preview, atomic save and conflict detection.
3. **Federation review:** explain resolution and full update/pin review with an
   explicit apply workflow and adapter-owned materialisation.
4. **Agent evidence:** only if actual consumption records exist; never relabel
   a retrieval preview as something an agent definitely read.

The next acceptance gate is installation and keyboard use on a real Omarchy
Wayland session. Headless Qt rendering and bridge tests do not prove compositor,
file-association, launcher or platform-dialog behaviour there.
