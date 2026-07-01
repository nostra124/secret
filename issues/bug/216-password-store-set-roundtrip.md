---
id: FEAT-216
type: bug
priority: high
status: open
milestone: 0.15.0
---

# `password-store` `set`/`get` do not round-trip after `pass-init`

## Description

**As a** user who has run `secret pass-init` and stores entries in the
shared password-store
**I want** `secret set pass/<name>` and `secret get pass/<name>` to
round-trip the same value
**So that** the `pass(1)` integration (FEAT-041) is actually usable and
stays interoperable with the `pass` CLI.

The entry-naming convention carried verbatim from the shell version does
not match what `pass insert` writes on disk, so a value written through
`secret` is stored under a path that `secret get` (and `pass show`) then
fail to resolve. The value is not lost, but it is addressed by a
different name on read than on write.

## Steps to reproduce

```bash
secret pass-init
printf 's3cr3t' | secret set pass/email/work
secret get pass/email/work        # ← empty / not found
pass show email/work              # ← also not found under that path
```

## Root cause

The `pass`-backed path builds the entry name with the legacy
`command:pass-insert` naming (store-prefixed, `:`→`/` translated) rather
than the `<name>.gpg` layout `pass` itself uses under
`$PASSWORD_STORE_DIR`. Write and read compute the leaf name differently,
so the round-trip breaks. This must be fixed with a failing unit test
first per `skills/bugs.md`.

## Implementation

1. Add a failing unit test (`tests/unit/pass.bats`, tool-free where
   possible, else SIT-gated) that writes a value via `secret set
   pass/<name>` and asserts `secret get pass/<name>` returns it.
2. Make the entry-name computation identical on the write and read
   paths, matching `pass`'s on-disk `<name>.gpg` layout so `pass show`
   and `secret get` agree.
3. Add an assertion that `pass show <name>` (SIT) sees the same value,
   pinning interoperability.

## Acceptance Criteria

1. `secret set pass/<name>` followed by `secret get pass/<name>`
   returns the written value.
2. The same entry is visible to the `pass` CLI at the equivalent path.
3. A unit test reproduces the pre-fix failure and passes after the fix.
4. Existing `tests/unit/secret.bats` contract is unchanged (no major
   version bump required).
