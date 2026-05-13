---
id: FEAT-030
type: feature
priority: high
status: resolved
resolved-in: 0.13.0
resolved-by: FEAT-215 (docs/secret.md). The last open criterion at the 2026-05-13 audit was the docs/secret.md gap; FEAT-215 closed it. Criterion #6 (docs/templates/CLAUDE.md.secret) is satisfied by CLAUDE.md at the repo root — see audit-2026-05-13.md.
---

# Secret self-contained packaging: docs, tests, man page, completion, CLAUDE.md

## Description

**As a** maintainer about to extract `secret` to its own rpk repo
**I want** every artefact a standalone rpk repo expects — man page,
unit tests, CLI contract doc, bash completion, `CLAUDE.md` —
present and verified inside this repo first
**So that** the extraction (FEAT-031) is a mechanical move of files.

Mirrors FEAT-023 (account) and FEAT-027 (config).

## Implementation

Depends on FEAT-029 (`secret` calls only `account` / `config` /
optionally `user` at runtime), FEAT-024 (`account` extracted),
FEAT-028 (`config` extracted). All should land before this.

For `bin/secret`:

1. **Move `etc/scripts/secret/*` → `libexec/secret/*`** per
   FEAT-001 (specialised here if FEAT-001 hasn't completed
   collection-wide). No inlining.
2. **`docs/secret.md`** per FEAT-004's template: synopsis,
   description, every subcommand with args / env / exit codes,
   environment variables, files, exit codes, cross-script
   dependencies (after FEAT-029: `account`, `config`, optionally
   `user` at runtime; `rpk` as deployment-only).
3. **`tests/unit/secret.bats`** per FEAT-003: covers every
   documented subcommand including failure-mode exit codes;
   sandboxed `$HOME` per test.
4. **`share/man/man1/secret.1`** (groff): NAME, SYNOPSIS,
   DESCRIPTION, SUBCOMMANDS, ENVIRONMENT, FILES, EXIT STATUS,
   EXAMPLES, SEE ALSO. Seeded from `bin/secret`'s `help` output and
   `docs/secret.md`.
5. **`etc/bash_completion.d/secret`** — already exists; verify it
   completes every subcommand currently exposed by `bin/secret help`
   and extend if not.
6. **`docs/templates/CLAUDE.md.secret`** — already drafted in
   FEAT-029; finalise here to match the actual extracted file
   layout.

## Acceptance Criteria

1. `bin/secret` contains zero `scripts has` / `scripts list`
   invocations; its dispatcher looks under `libexec/secret/`
   directly.
2. `docs/secret.md` exists and lists every subcommand from
   `secret help`.
3. `bats tests/unit/secret.bats` passes.
4. `man -l share/man/man1/secret.1` renders with all sections
   populated.
5. Tab completion works for `secret <TAB>` and for every subcommand
   that takes arguments.
6. `docs/templates/CLAUDE.md.secret` exists, references the
   foundation template, and reflects the actual layout for the
   extracted repo.
