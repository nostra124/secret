---
id: FEAT-042
type: feature
priority: medium
status: open
---

# `secret(1)` man page

## Description

**As a** user of `secret`
**I want** a man page that documents every subcommand, the multi-
store layout, the GPG-at-rest model, and the git-backed sync flow
**So that** `man secret` is a complete reference; users don't have
to read source to understand the tool.

`secret` is **not** an educational script in the
crypt/bitcoin sense (no vendored standards, no agent skill, no
walkthrough), but a man page is non-negotiable for any extracted
package.

## Implementation

`share/man/man1/secret.1` (groff). Sections: NAME, SYNOPSIS,
DESCRIPTION, **STORES**, **SUBCOMMANDS**, ENVIRONMENT, FILES,
EXIT STATUS, EXAMPLES, SEE ALSO.

- **DESCRIPTION** explains the multi-store, hierarchical-key model
  (`<store>/<param>` with colon-separated parameter IDs,
  per-user vs. per-system stores, the special `pass` store).
- **STORES** documents the precedence rules:
  - Root user → `/etc/secret/<store>`
  - Normal user → `${XDG_CONFIG_HOME}/secret/<store>`
  - `XDG_SECRET_STORES` override
- **SUBCOMMANDS** groups by purpose (matching the existing
  `secret help` text):
  - generic: help, version
  - store management: stores, params, init, exists, destroy, clean,
    pass-init (FEAT-041)
  - parameter ops: ls, has, set, get, del, qr, gen, def, edit,
    ins, rem
  - synchronisation: remotes, pull, push, sync (semantics
    documented to match repo and bitcoin push/pull per FEAT-043)
  - GPG keys: gpg-keys, add-gpg-key, del-gpg-key
- **EXAMPLES** covers the common flows: init a store, set/get a
  param, push to another account, recover from a fresh machine.
- **SEE ALSO** links: `gpg(1)`, `pass(1)`, `account(1)`,
  `config(1)`.

The man page is **co-authoritative with `tests/unit/secret.bats`
(FEAT-030)**: every subcommand listed in the man page has at
least one test case; if a subcommand is added, both must change
together. A simple harness check in `tests/unit/secret.bats`
parses the man page's SUBCOMMANDS section and asserts each name
matches a `command:*` function.

## Acceptance Criteria

1. `man secret` renders with all sections populated.
2. Every subcommand in `bin/secret`'s `command:*` functions
   appears in the man page's SUBCOMMANDS section, and vice versa.
3. The man-vs-tests cross-check in `tests/unit/secret.bats`
   passes: every subcommand documented has a corresponding test
   case.
4. `share/man/man1/secret.1` ships in the extracted repo
   (FEAT-031 layout updated to include it).
5. SEE ALSO links resolve to installed man pages on a machine
   with the full collection.
