---
version: 0.10.0
milestone: test-coverage
from: 0.9.0
status: in-progress
semver-justification: >
  Minor bump. FEAT-204 adds new bats tests (expands the contract).
  FEAT-203 fixes unquoted expansions — no user-visible behaviour change
  on typical paths, but stores with spaces in names now work.
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

- [ ] `bats tests/unit/secret.bats` — all tests green (≥57 after FEAT-204)
- [ ] `shellcheck -S warning bin/secret` — zero SC2086/SC2046 warnings (FEAT-203)
- [ ] FEAT-207 policy decision recorded in issue file before merge
- [ ] If FEAT-207 Option A: `secret exists ../foo` exits non-zero with `fatal` message
- [ ] If FEAT-207 Option A: version bumped to 1.0.0 (breaking change)
- [ ] `.rpk/version` bumped to `0.10.0` (or `1.0.0`) and entry added to `.rpk/versions`
