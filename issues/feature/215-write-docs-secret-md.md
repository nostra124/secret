---
id: FEAT-215
type: feature
priority: medium
status: open
follows: FEAT-030
discovered-by: audits/audit-2026-05-13.md
---

# Write `docs/secret.md` — the CLI contract reference

## Description

**As a** consumer of `secret` (a script writer or operator)
**I want** a single Markdown file documenting every subcommand,
its arguments, environment variables, and exit codes
**So that** I can wrap `secret` in scripts without reading
`bin/secret` end-to-end or running `secret help` repeatedly.

FEAT-030's acceptance criterion #2 calls for `docs/secret.md` —
the audit on 2026-05-13 found it missing while every other
FEAT-030 criterion (man page, bash completion, bats suite, no
`scripts` calls, CLAUDE.md template) was met. Filing this issue
to close the gap separately so FEAT-030 can be re-evaluated.

## Implementation

1. Create `docs/secret.md` following FEAT-004's template
   (per FEAT-030's implementation §2):
   - SYNOPSIS
   - DESCRIPTION
   - For each subcommand from `secret help`: name, args, env vars
     read, exit codes, examples
   - ENVIRONMENT (`$XDG_SECRET_STORES`, `$SELF_DEBUG`,
     `$SELF_QUIET`, `$SELF_FORCE`, `$EDITOR`, `$HOME`,
     `$XDG_CONFIG_HOME`)
   - FILES (`$SELF_CONFIG/<store>/<param>.gpg`,
     `$HOME/.password-store/`, `.rpk/version`, `VERSION`)
   - EXIT STATUS (0 success, non-zero on `fatal`, see
     `skills/logging.md` for the levels)
   - CROSS-SCRIPT DEPENDENCIES: `account` (runtime), `git`,
     `gpg`, `pass`, `qrencode`, `ssh`
2. Cross-link from `Readme.md` ("Documentation" section already
   points at `docs/secret.md` but the link 404s).
3. Verify the document matches `secret help` output by spot-check.

## Acceptance Criteria

1. `docs/secret.md` exists at the project root.
2. Every subcommand from `secret help` has a section in the doc.
3. Every environment variable read by `bin/secret` has an entry
   in the ENVIRONMENT section.
4. The `Readme.md` link to `docs/secret.md` resolves.
5. Re-running the FEAT-030 acceptance-criteria check shows all 6
   items green; FEAT-030 can then be flipped to `resolved` (with
   a `resolved-in` matching the release that ships this issue).

## Notes

Discovered via `skills/audit.md` traceability check on 2026-05-13.
Not assigned to a milestone yet — file under `ROADMAP-0.13.0.md`
when the next milestone is opened.
