---
id: FEAT-206
type: feature
priority: low
status: open
---

# Tighten loose assertions in existing unit tests

## Description

**As a** maintainer reading test failures
**I want** each test to assert only what it claims to assert
**So that** a failing test points directly at the broken behaviour without ambiguity.

Several existing tests in `tests/unit/secret.bats` either omit an exit-status check
or check the output too loosely. Specific issues:

### Missing status checks

| Test | Current gap |
|---|---|
| `secret help prints usage` (test 3) | No `[ "$status" -eq 0 ]` |
| `secret with no args prints help` (test 4) | No `[ "$status" -eq 0 ]` |
| `help mentions store management commands` (test 7) | No status check |
| `help mentions store parameter commands` (test 8) | No status check |
| `help mentions account encryption commands` (test 9) | No status check |
| `help mentions sync commands group` (test 10) | No status check |

If `command:help` ever starts returning non-zero (e.g. a new early-exit path is
added), these tests pass anyway, hiding the regression.

### Version test does not verify content

Test 2 (`secret version returns a non-empty string`) checks `[ -n "$output" ]` but
does not verify the output matches `.rpk/version`. The FEAT-194 contract is that
the binary reads its version from `.rpk/version` (dev tree) or
`share/secret/version` (installed). A test should pin this:

```bash
expected=$(cat "$BATS_TEST_DIRNAME/../../.rpk/version")
[ "$output" = "$expected" ]
```

### Stderr separation

Test 5 (`unknown subcommand exits non-zero with fatal message`) matches `*"fatal"*`
against `$output`, which bats populates with combined stdout+stderr. The `fatal`
function writes to stderr only (`>&2`). The assertion is accidentally correct today
but will silently break if `fatal` is changed to write to stdout, or if bats is
invoked in a mode that separates them. Use `run --separate-stderr` (bats ≥1.5) and
assert against `$stderr`.

### Syncronisation typo mask (depends on FEAT-201)

Test 10 uses `*"yncronisation commands"*` to avoid committing to a spelling. Once
FEAT-201 fixes the typo, update to `*"synchronisation commands"*`.

## Implementation

1. Add `[ "$status" -eq 0 ]` to tests 3, 4, 7, 8, 9, 10.
2. Add version-content assertion to test 2.
3. Replace the combined-output stderr check in test 5 with
   `run --separate-stderr "$SECRET_BIN" definitely-not-a-real-subcommand` and assert
   `[[ "$stderr" == *"fatal"* ]]` (or keep `$output` and document the choice).
4. After FEAT-201 lands, update test 10 to `*"synchronisation commands"*`.

## Acceptance Criteria

1. Tests 3, 4, 7, 8, 9, 10 each contain a `[ "$status" -eq 0 ]` assertion.
2. Test 2 verifies `$output` equals the contents of `.rpk/version`.
3. Test 5's fatal-message assertion targets stderr (or documents why stdout is
   intentional).
4. Test 10 matches the correctly-spelled `synchronisation` after FEAT-201.
5. All 40 existing tests still pass.
