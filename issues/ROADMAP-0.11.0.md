---
version: 0.11.0
milestone: polish
from: 0.10.0
status: released
semver-justification: >
  Minor bump. FEAT-209 changes the observable output of `secret ls`
  (case normalisation). FEAT-208 changes the output order of
  `secret params password-store`. FEAT-210 adds a new subcommand
  (`secret clean`). FEAT-211, FEAT-212 fix exit-code behaviour for
  remotes/ins/rem (no existing bats assertion broken). FEAT-214
  is a process change (skill update) with no code impact.
---

# Release 0.11.0 — polish

> Completes the test suite (dispatcher shortcut, `destroy
> password-store`, tightened assertions), normalises two
> long-standing output inconsistencies, fixes three bugs
> discovered during 0.10.0, and codifies the wake-time
> reconciliation rule.

## Issues

| ID | Type | Title | Priority |
|---|---|---|---|
| [FEAT-214](feature/214-reconcile-pr-state-on-every-wake.md) | feature | Reconcile PR state on every wake, not just on webhooks | high |
| [FEAT-210](bug/210-command-clean-not-implemented.md) | bug | `command:clean` advertised in help but not implemented | medium |
| [FEAT-211](bug/211-remotes-no-args-infinite-recursion.md) | bug | `command:remotes` infinite-recurses with no args and no stores | medium |
| [FEAT-212](bug/212-ins-rem-mask-exit-code.md) | bug | `command:ins` and `command:rem` mask all error exits via `return 0` | medium |
| [FEAT-205](feature/205-test-dispatcher-destroy-flags.md) | feature | Add tests for `store/param` dispatcher shortcut, `destroy password-store`, and flag parsing | medium |
| [FEAT-206](feature/206-tighten-existing-test-assertions.md) | feature | Tighten loose assertions: add status checks, verify version content, separate stderr | low |
| [FEAT-208](feature/208-normalise-params-sort.md) | feature | Normalise `sort -u` in `command:params` for password-store | low |
| [FEAT-209](feature/209-normalise-ls-case-sensitivity.md) | feature | Normalise `ls` prefix case handling vs. `stores` listing | low |

FEAT-213 (deep SC2086 cleanup, 225 instances) shuffled to
[ROADMAP-0.12.0.md](ROADMAP-0.12.0.md) — too large to absorb
without slipping this milestone (`skills/milestones.md` §3,
§4).

## Dependencies

- FEAT-205 (flag-parsing tests) requires FEAT-202 from 0.9.0 — satisfied.
- FEAT-206 (synchronisation pattern) requires FEAT-201 from 0.9.0 — satisfied.

## Bats contract impact

| Change | Impact |
|---|---|
| FEAT-214 | Skill-only; no test impact |
| FEAT-210 | Adds 3 new tests + new subcommand |
| FEAT-211 | Adds 1 test, fixes infinite recursion |
| FEAT-212 | Adds 4 tests, hoists guards |
| FEAT-205 | Adds 4 new tests (dispatcher shortcut + destroy password-store) |
| FEAT-206 | Tightens existing assertions; adds 1 version-pin test |
| FEAT-208 | Adds 1 test; output order change |
| FEAT-209 | Adds 1 test; case-insensitive `ls` prefix |

## Release checklist

- [x] `bats tests/unit/secret.bats` — 83 tests green (+15 over 0.10.0's 68)
- [x] `secret help` exit status is 0 (FEAT-206)
- [x] `secret version` output equals contents of `VERSION` (FEAT-206)
- [x] `secret params password-store` output is sorted (FEAT-208)
- [x] `secret ls Alpha` and `secret ls alpha` return the same results (FEAT-209)
- [x] `secret remotes` on a fresh install exits in <1s with no output (FEAT-211)
- [x] `secret clean <store>` removes empty `.gpg` files (FEAT-210)
- [x] `echo | secret ins` exits non-zero (FEAT-212)
- [x] `skills/automerging.md` §2c documents wake-time reconciliation (FEAT-214)
- [x] `VERSION` and `.rpk/version` bumped to `0.11.0`; `.rpk/versions` ledger appended
