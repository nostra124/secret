---
id: FEAT-201
type: bug
priority: medium
status: resolved
resolved-in: 0.9.0
---

# Fix typos in user-visible strings throughout `bin/secret`

## Description

**As a** user reading error messages and help text
**I want** correct English spelling
**So that** messages are professional, searchable in documentation, and not masked by
workarounds in the test suite.

Several misspellings exist in `bin/secret` today. One of them (`syncronisation`) is
actively papered over in the unit test at line 94 of `tests/unit/secret.bats` with a
pattern that drops the leading letter, hiding the bug from reviewers.

## Inventory of misspellings

| Location | Current | Correct |
|---|---|---|
| `bin/secret:152` (help) | `syncronisation` | `synchronisation` |
| `bin/secret:832` (fatal) | `unkown command` | `unknown command` |
| `bin/secret:745,747,759` (add-gpg-key) | `unkown account` | `unknown account` |
| `bin/secret:405` (get) | `paramenter` | `parameter` |
| `bin/secret:144` (help) | `randowm` | `random` |
| `bin/secret:258` (init) | `gpg-import-publi-key` | `gpg-import-public-key` |

The last entry (`gpg-import-publi-key`) is likely a broken call to `account` and may
be a functional bug, not just cosmetic — verify whether `account gpg-import-public-key`
is the correct invocation.

## Implementation

1. Apply the corrections in `bin/secret` at the line numbers above.
2. Update the corresponding assertion in `tests/unit/secret.bats` line 94 from:
   ```bash
   [[ "$output" == *"yncronisation commands"* ]]
   ```
   to:
   ```bash
   [[ "$output" == *"synchronisation commands"* ]]
   ```
3. Verify `gpg-import-publi-key` against the `account` CLI; fix the call to match the
   actual subcommand name.

## Acceptance Criteria

1. `secret help` output contains `synchronisation` (9 letters).
2. `secret unknown-cmd` stderr contains `unknown command`.
3. `secret add-gpg-key store noone` stderr contains `unknown account`.
4. `secret get nostore/param` stderr contains `parameter` (not `paramenter`).
5. `secret help` output contains `random` (in the `gen` description line).
6. All existing unit tests still pass after the string corrections.
7. The `bats` assertion at `tests/unit/secret.bats:94` matches the full word
   `synchronisation`.
