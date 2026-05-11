---
id: FEAT-204
type: feature
priority: high
status: open
---

# Add missing arg-validation unit tests for `gen`, `def`, `ins`, `rem`, `clean`, and GPG-key subcommands

## Description

**As a** maintainer keeping the bats contract stable
**I want** argument-validation tests for every subcommand that has early-exit paths
**So that** regressions in validation logic are caught immediately, without requiring
GPG, `pass`, or network access.

The current test suite covers validation for `set`, `get`, `del`, `qr`, `edit`,
`has`, `exists`, `params`, `destroy` — but not for `gen`, `def`, `ins`, `rem`,
`clean`, `add-gpg-key`, `del-gpg-key`, `has-gpg-key`, or `remotes`. All of these
have early-exit paths (missing arg, wrong format) that are pure bash and safe to
exercise in a unit test.

## Implementation

Add a new section to `tests/unit/secret.bats` for each group below. Each test
follows the existing pattern: `run "$SECRET_BIN" <verb> [args]`, assert `status`
and `output`.

### `gen` / `def`
```bash
@test "gen without arg exits non-zero"
@test "gen without slash separator exits non-zero"
@test "def without arg exits non-zero"
@test "def without slash separator exits non-zero"
```

### `ins` / `rem`
```bash
@test "ins without arg exits non-zero"
@test "rem without arg exits non-zero"
```

### `clean`
```bash
@test "clean without arg exits non-zero"
@test "clean on missing store exits 0 (no-op)"
```

### `remotes`
```bash
@test "remotes with no stores returns empty success"
@test "remotes with a store that has no git remotes returns empty success"
```

### `add-gpg-key` / `del-gpg-key` / `has-gpg-key`
```bash
@test "add-gpg-key without store arg exits non-zero"
@test "add-gpg-key without account arg exits non-zero"
@test "del-gpg-key without store arg exits non-zero"
@test "del-gpg-key without account arg exits non-zero"
@test "has-gpg-key without store arg exits non-zero"
@test "has-gpg-key without account arg exits non-zero"
@test "has-gpg-key returns non-zero when store lacks key file"
```

## Acceptance Criteria

1. All new tests pass (`bats tests/unit/secret.bats`).
2. No new external dependencies introduced in the test setup.
3. Test count increases from 40 to at least 57 (17 new tests).
4. Any future removal of a validation `fatal` call in a covered subcommand causes
   at least one test to fail.
