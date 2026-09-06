# Authoring milestone

Authorised direction, 6 September 2026: continue toward a usable native knowledge
workspace with an Obsidian-like editing experience. This supersedes the initial
read-only milestone for explicit local editing and creation only.

The design uses a charcoal workspace, warm paper-coloured text, a restrained
amber accent, a file navigator, document tabs, a split source/preview editor and
a decision-context inspector. The inspector is the signature interaction: it
connects the document to its recorded relationships and provenance. Qt controls,
keyboard focus and native window behaviour remain intact.

## Write contract

- Reads return exact UTF-8 bytes plus their SHA-256 fingerprint.
- Existing-file writes retain the canonical ID and artifact type. Changing an
  identity or silently converting an artifact is not an editor operation.
- Review validates the draft through core's structural and source-aware proposed
  document validation, and presents a textual diff. Apply is a separate explicit
  action and revalidates rather than trusting the prior review.
- Save refuses missing, changed, symlinked or inherited paths. The file is written
  through a sibling temporary file, synced, and atomically renamed; permissions
  are preserved. An advisory per-project lock serialises this app's writers.
  External editors do not participate in that lock; their changes are checked
  immediately before replacement, not controlled by a global filesystem lock.
- New drafts use core templates and core-generated IDs. Creation uses no-clobber
  persistence and never replaces an existing file. Parent directories must exist.
- Structural errors block saving. Warnings remain visible. The preview is a local
  rendering of the draft; no embedded remote resources are loaded.
- Dirty buffers survive switching tabs. Closing a dirty tab, switching projects,
  or quitting requires an explicit discard choice. Save and conflict failures
  retain the draft for correction or copying.
- Unsaved buffers are debounced to owner-only local recovery files outside the
  repository. Reopening restores them with their original fingerprint. Explicit
  discard removes the buffer; recovery never silently writes corpus files.

Source updates, moving/renaming artifacts, automatic Git commits, deletion and
multi-file supersession transactions remain outside this authoring milestone.
Those need their own review semantics. The editor can amend lifecycle sections
and relationship declarations through the same reviewed save path.
