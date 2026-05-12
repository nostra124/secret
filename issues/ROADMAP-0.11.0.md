---
version: 0.11.0
milestone: polish
from: 0.10.0
semver-justification: >
  Minor bump. FEAT-209 changes the observable output of `secret ls`
  (case normalisation). FEAT-208 changes the output order of
  `secret params password-store`. Both are technically output-contract
  changes, hence minor rather than patch.
---

# Release 0.11.0 — polish

> Completes the test suite (dispatcher shortcut, `destroy
> password-store`, tightened assertions) and normalises two long-
> standing output inconsistencies. Sprint 3 issues only. Deferred if
> sprint capacity is tight — none are regressions.

## Issues

| ID | Type | Title | Priority |
|---|---|---|---|
| [FEAT-210](bug/210-command-clean-not-implemented.md) | bug | `command:clean` advertised in help but not implemented | medium |
| [FEAT-211](bug/211-remotes-no-args-infinite-recursion.md) | bug | `command:remotes` infinite-recurses with no args and no stores | medium |
| [FEAT-212](bug/212-ins-rem-mask-exit-code.md) | bug | `command:ins` and `command:rem` mask all error exits via `return 0` | medium |
| [FEAT-213](feature/213-deep-sc2086-quoting-cleanup.md) | feature | Deep SC2086 quoting cleanup (225 instances; follow-up to FEAT-203) | low |
| [FEAT-205](feature/205-test-dispatcher-destroy-flags.md) | feature | Add tests for `store/param` dispatcher shortcut, `destroy password-store`, and flag parsing | medium |
| [FEAT-206](feature/206-tighten-existing-test-assertions.md) | feature | Tighten loose assertions: add status checks, verify version content, separate stderr | low |
| [FEAT-208](feature/208-normalise-params-sort.md) | feature | Normalise `sort -u` in `command:params` for password-store | low |
| [FEAT-209](feature/209-normalise-ls-case-sensitivity.md) | feature | Normalise `ls` prefix case handling vs. `stores` listing | low |

## Dependencies

- FEAT-205 (flag-parsing tests) requires FEAT-202 from 0.9.0.
- FEAT-206 (assertion tightening, syncronisation pattern) requires
  FEAT-201 from 0.9.0.
- FEAT-208 and FEAT-209 are independent; can be cherry-picked earlier
  as patch releases (0.10.1, 0.10.2) if desired.

## Bats contract impact

| Change | Impact |
|---|---|
| FEAT-205 adds 6 new tests | Expands contract; no existing assertions change |
| FEAT-206 tightens existing assertions | Existing tests become stricter; any future regression in `help` exit code or version output will be caught |
| FEAT-206 updates syncronisation pattern to full word | Depends on FEAT-201 having been applied; test would fail on an un-patched binary |
| FEAT-208 adds `sort -u` to password-store params | Output order change for `secret params password-store` |
| FEAT-209 normalises `ls` case | `secret ls Alpha` now returns results (was silent); `secret ls alpha` still works |

## Release checklist

- [ ] `bats tests/unit/secret.bats` — all tests green (≥63 after FEAT-205)
- [ ] `secret help` exit status is 0 (FEAT-206 will catch it if not)
- [ ] `secret version` output equals contents of `.rpk/version` (FEAT-206)
- [ ] `secret params password-store` output is sorted (FEAT-208)
- [ ] `secret ls Alpha` and `secret ls alpha` return the same results (FEAT-209)
- [ ] `.rpk/version` bumped to `0.11.0` and entry added to `.rpk/versions`
