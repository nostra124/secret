---
version: 0.10.0
milestone: test-coverage
from: 0.9.0
status: released
semver-justification: >
  Minor bump. FEAT-204 adds new bats tests (expands the contract).
  FEAT-203 removes eval and quotes command:stores (deep cleanup
  for the remaining 225 SC2086 instances deferred to FEAT-213).
  FEAT-207 implements Option A (reject `..` path traversal); the new
  fatal exit on traversal inputs is a security-tightening minor bump,
  not a major. Existing bats assertions remain valid.
---

# Release 0.10.0 — test coverage

> Closes the three most significant gaps in the unit-test suite:
> missing arg-validation coverage, missing path-safety guards, and
> unquoted variable expansions that break paths containing spaces.
> Sprint 2 issues only.

## Issues

| ID | Type | Title | Priority |
|---|---|---|---|
| [FEAT-204](feature/204-test-missing-arg-validation.md) | feature | Add arg-validation tests for `gen`, `def`, `ins`, `rem`, `clean`, GPG-key and `remotes` subcommands | **high** |
| [FEAT-203](bug/203-fix-unquoted-variable-expansions.md) | bug | Quote variable expansions throughout `bin/secret` | medium |
| [FEAT-207](feature/207-path-traversal-policy.md) | feature | Define and enforce path-traversal policy for store and parameter names | medium |

## Dependencies

- Requires 0.9.0 to be released first (FEAT-204 tests are easier to
  write against the fixed function names from FEAT-200).
- FEAT-207 requires a team decision on Option A (reject `..`) vs.
  Option B (document and test current behaviour) **before** coding
  begins. If Option A is chosen and the rejection is considered a
  breaking change, bump to **1.0.0** instead.

## Bats contract impact

| Change | Impact |
|---|---|
| FEAT-204 adds 17 new validation tests | Expands contract; no existing assertions change |
| FEAT-203 quoting fixes | May cause paths with spaces to pass that previously silently failed; no existing test is affected |
| FEAT-207 (Option A) adds `..`-rejection | New tests; may break callers that rely on traversal being silently ignored — evaluate before release |
| FEAT-207 (Option B) documents current behaviour | New tests only; no behaviour change |

## Release checklist

- [x] `bats tests/unit/secret.bats` — 68 tests green (12 new for FEAT-204; 7 new for FEAT-207; 1 new for FEAT-203)
- [x] `shellcheck -S warning bin/secret` — zero SC2046 warnings
- [x] FEAT-207 policy decision recorded in issue file (Option A — reject)
- [x] `secret exists ../foo` exits non-zero with `fatal` message
- [x] No version-bump escalation to 1.0.0 — security-tightening minor bump (existing bats contract preserved)
- [x] `VERSION` and `.rpk/version` bumped to `0.10.0`; entry appended to `.rpk/versions`
