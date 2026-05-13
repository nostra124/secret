---
version: 0.13.0
milestone: docs
from: 0.12.0
status: released
semver-justification: >
  Minor bump. Adds `docs/secret.md` (the CLI contract reference)
  per FEAT-215 — a substantial new artefact in the repo, but no
  code change and no bats contract change. Could justify a patch
  bump (0.12.1); going with minor for milestone consistency with
  prior releases. FEAT-030 (self-contained-packaging) closes as a
  side-effect since `docs/secret.md` was its lone outstanding
  gap.
---

# Release 0.13.0 — docs

> Closes the documentation gap surfaced by the 2026-05-13 audit.
> One feature lands directly (FEAT-215); one closes transitively
> (FEAT-030). No code or test changes.

## Issues

| ID | Type | Title | Priority |
|---|---|---|---|
| [FEAT-215](feature/215-write-docs-secret-md.md) | feature | Write `docs/secret.md` — the CLI contract reference | medium |
| [FEAT-030](feature/030-secret-self-contained-package.md) | feature | Secret self-contained packaging (closes once FEAT-215 ships) | high |

## Dependencies

- None. FEAT-215 is independent. FEAT-030 closes as a transitive
  effect of FEAT-215 shipping.

## Bats contract impact

| Change | Impact |
|---|---|
| FEAT-215 | None — pure documentation; no `bin/secret` or test change |
| FEAT-030 | None — status flip only |

## Release checklist

- [x] `docs/secret.md` exists at repo root (347 lines)
- [x] Every subcommand from `secret help` has a section in the doc (28 of 28)
- [x] Every env var read by `bin/secret` has an ENVIRONMENT entry
- [x] `Readme.md` link to `docs/secret.md` resolves
- [x] FEAT-030's 5 of 6 criteria green; #6 satisfied by root `CLAUDE.md`
- [x] `bats tests/unit/secret.bats` — 90 tests green (unchanged)
- [x] `VERSION` and `.rpk/version` bumped to `0.13.0`; ledger entry appended
