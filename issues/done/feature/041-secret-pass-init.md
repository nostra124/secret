---
id: FEAT-041
type: feature
priority: medium
status: resolved
resolved-in: 0.9.0
resolved-by-audit: audits/audit-2026-05-13.md
---

# Absorb `pass-setup` from `crypt` into `secret`

## Description

**As a** maintainer
**I want** the `pass(1)` initialisation flow that lived in `crypt`
to land in `secret` instead
**So that** secret-management logic stays inside the secret manager,
and `crypt` (the cryptography primitives tool) doesn't grow scope
back into secret-store concerns.

FEAT-032 removes `pass-setup` from `crypt` on the rationale that
`pass(1)` is a secret manager and its bootstrap belongs in `secret`.
This ticket makes that drop non-destructive by giving the
functionality a new home with the same capability.

## Implementation

Add `command:pass-init` to `bin/secret` (name chosen for parity
with secret's existing `init <store>` verb). Behaviour:

1. Verify `gpg` and `pass` are reachable on `$PATH`; fail with a
   clear message naming the missing OS package if not (parity with
   crypt's soft-dependency probe pattern).
2. If the user has no GPG identity yet, prompt for one and create
   it (delegating to `crypt gpg-create` if `crypt` is on `$PATH`,
   or using the inline `gpg --quick-generate-key` otherwise — no
   hard `secret → crypt` dep).
3. Initialise the password store: `pass init <gpg-id>` against
   the user's `${HOME}/.password-store` (the path secret already
   knows about — see `command:pull` handling).
4. Seed the secret-side `pass` store wrapper so subsequent
   `secret get pass/<key>` calls work.

Help text under `secret help pass-init` documents the flow and
references `pass(1)`.

The corresponding entry in the (new) `secret(1)` man page
(FEAT-042) describes pass-init alongside the other store-management
subcommands.

## Acceptance Criteria

1. `secret pass-init` initialises a password store on a fresh
   machine with no prior gpg identity, end-to-end.
2. `secret pass-init` against an already-initialised store is a
   safe no-op (idempotent), with a clear "already initialised"
   info message.
3. `secret get pass/<key>` works after `pass-init` for any key
   the user subsequently inserts via `pass insert`.
4. `crypt help pass-setup` no longer exists (FEAT-032 removed it);
   `secret help pass-init` does.
5. `tests/unit/secret.bats` (per FEAT-030) covers `pass-init`'s
   happy path and its idempotent re-run.
6. The `secret(1)` man page (FEAT-042) lists `pass-init`.
