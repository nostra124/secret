---
name: milestones
description: |
  Plan, run, and retire a `secret` milestone. Trigger when a
  release is being scoped (new `ROADMAP-<x.y.z>.md`), when a
  feature is being shuffled between milestone backlogs, when a
  milestone has just shipped, or when the user asks "what's
  planned for the next version" or "how do I add this to a
  future release". Companion to `skills/version.md` (the
  mechanics of bumping the version), `skills/features.md` and
  `skills/bugs.md` (issue authoring), and `skills/audit.md`
  (the periodic traceability check).
---

# `milestones` skill — backlog version files as a central plan

## 0. The concept

> **`issues/ROADMAP-<x.y.z>.md` is the planning artefact.** Every
> issue lives in `issues/bug/` or `issues/feature/`, and every
> issue is referenced from exactly one `ROADMAP-<x.y.z>.md` file
> until it ships. There is no separate spreadsheet, project
> board, or tracker — the roadmap files in the repo are the
> single source of truth.

Each milestone file is a target release. The presence of
`ROADMAP-0.10.0.md` in the tree means "0.10.0 is the next planned
release and these are the issues that go into it". When 0.10.0
ships, the file is deleted (history is preserved in git) and any
remaining issues are shuffled into the next milestone file.

This is intentionally lightweight. Planning is `git add`, retiring
is `git rm`, and provenance is `git log`.

## 1. Layout

```
issues/
├── bug/                    open + in-progress bugs (type: bug)
│   └── <NNN>-<slug>.md
├── feature/                open + in-progress features (type: feature)
│   └── <NNN>-<slug>.md
├── done/                   resolved issues, kept for traceability
│   ├── bug/
│   │   └── <NNN>-<slug>.md
│   └── feature/
│       └── <NNN>-<slug>.md
├── ROADMAP-<x.y.z>.md      planned milestone (one per future version)
└── …
```

`issues/done/` is the archive — `git log --follow` on any file
there reproduces the full history from open to resolved. A file
that has never moved through `done/` is either still active
(in `bug/` or `feature/`) or has been deleted (a no-op issue that
never shipped — rare; record the rationale in the deletion
commit).

## 2. Authoring a milestone

```
1. Pick the next version per skills/version.md §2 (semver rules).
2. Create issues/ROADMAP-<x.y.z>.md with frontmatter:
       version, milestone, from, status: planned,
       semver-justification.
3. Assign issues to it:
   - List each FEAT-NNN under the correct category (bugs / test
     improvements / code quality / etc.) with the priority and
     dependency notes.
   - Add the bats-contract-impact table — any existing assertion
     that the milestone will break must be flagged.
4. Add the release checklist at the bottom: empty checkboxes for
   every observable thing that must hold by the time the
   milestone ships.
5. Commit. The file is now the plan.
```

Example: `issues/ROADMAP-0.10.0.md`. Re-read it after authoring
yours to confirm shape.

## 3. Running a milestone (one session per milestone)

> **One milestone per session run.** Open the milestone file,
> work through its issues in dependency order, tick checkboxes as
> you go, and bump `VERSION` + `.rpk/version` at the end. Do not
> bundle two milestones into one session — they are deliberately
> sized to fit.

Per-session execution:

```
1. Open issues/ROADMAP-<x.y.z>.md. Set status: in-progress.
2. For each issue, in the order listed:
   - flip its frontmatter to status: in-progress
   - bug → follow skills/bugs.md (TDD: failing test first)
   - feature → follow skills/features.md (tests alongside)
   - run `make check-unit` locally; commit when green
   - flip the issue to status: resolved, resolved-in: <x.y.z>
3. Tick each release-checklist item as it's verified.
4. Bump VERSION and .rpk/version per skills/version.md.
5. Push. CI runs. Auto-merge or manual-merge per
   skills/automerging.md.
```

The mechanics are the same as any other PR — what's specific to
milestone work is the **scope discipline**: nothing outside the
roadmap file's issue list lands in this PR. If you discover a new
bug mid-session, file it and assign it to a future milestone, not
this one.

## 4. Shuffling features between milestones

When a feature moves from one planned milestone to another, the
shuffle must update **every** affected file in a single commit:

```
1. Remove the FEAT-NNN row from the source ROADMAP-<a.b.c>.md
   (table, sequencing notes, bats-contract-impact table).
2. Add the FEAT-NNN row to the destination ROADMAP-<x.y.z>.md
   with the same priority and updated dependency notes.
3. If the dependency graph changes (the feature now blocks /
   unblocks something in either milestone), update both files'
   "Dependencies" sections.
4. If the destination milestone changes priority or semver impact,
   re-derive the bats-contract-impact table and semver-
   justification in its frontmatter.
5. Commit with subject "Shuffle FEAT-NNN from <a.b.c> to <x.y.z>".
   No code changes in the shuffle commit.
```

Never edit only the source or only the destination. The roadmap
files are the plan — divergence between them creates ambiguity
about which release a feature is targeting.

## 5. Retiring a completed milestone

The moment a milestone ships (PR merged to `master`, tag pushed),
the next session opens with a cleanup commit:

```
1. git rm issues/ROADMAP-<x.y.z>.md
   - The file's history is preserved in git; no `done/` archive
     for roadmap files.
2. For every issue marked `resolved-in: <x.y.z>`:
   - git mv issues/bug/<NNN>-<slug>.md issues/done/bug/<NNN>-<slug>.md
     (or feature/ → done/feature/)
   - The issue file's frontmatter is unchanged; the move itself
     is the archival event.
3. Commit with subject "Retire <x.y.z>: archive resolved issues
   and remove ROADMAP file".
4. The next milestone's session continues from this clean state.
```

If any issue under the retiring milestone is **not** resolved,
it is a bug in the milestone scope, not the cleanup:

- Either flip it back to `status: open` and shuffle to a future
  milestone (see §4), or
- Add a follow-up patch release (`<x.y.z+1>`) that completes it.

Never leave a "released" milestone with unresolved issues in its
list; either delete the issue (rare) or move it.

## 6. Traceability invariant

Every behaviour the software exhibits should be traceable to one
or more issue files (open or done):

- A subcommand exists → trace to the feature that introduced it.
- A subcommand behaves a certain way → trace to the feature
  defining the behaviour or the bug that fixed a deviation.
- An error message exists → trace to a feature or bug.

This invariant is checked by the periodic audit — see
`skills/audit.md`. The audit is the gate that makes the
"`issues/` is the source of truth" claim hold over time.

## 7. Common mistakes

- **Roadmap files that drift from the actual issue files.** If
  `ROADMAP-0.10.0.md` lists FEAT-204 but the file's frontmatter
  says `status: resolved, resolved-in: 0.9.0`, one of the two
  is wrong. The roadmap files mirror the issue files' status —
  never edit one without the other.
- **Two milestones in flight at once.** Tempting if a small
  cosmetic fix doesn't fit the current milestone's theme. Don't —
  shuffle it to a future milestone instead.
- **Deleting an issue file when the work was never done.** That
  loses the "we considered this and decided no" provenance. Move
  to `done/` with a `status: rejected` frontmatter and a
  rationale note instead.
- **Skipping the retire commit.** Once a milestone has shipped,
  the very next commit is the §5 cleanup. Letting it linger means
  every audit run flags `ROADMAP-<shipped-version>.md` as a
  bookkeeping gap.

## 8. Cross-references

- `skills/version.md` — bumping `VERSION` and `.rpk/version` at
  release time; semver decision rules.
- `skills/features.md` — feature-issue authoring.
- `skills/bugs.md` — bug-issue authoring + TDD loop.
- `skills/automerging.md` — what happens once the milestone PR is
  pushed and CI goes green.
- `skills/audit.md` — periodic traceability check that keeps the
  `issues/` ↔ code mapping honest.
- `CLAUDE.md` §3 (issue authoring), §8 (versioning).
