---
version: 0.9.0
milestone: correctness
from: 0.8.0
semver-justification: >
  Minor bump. FEAT-201 changes user-visible error message strings
  (output contract); FEAT-202 changes flag-parsing behaviour. Neither
  breaks existing bats assertions (the syncronisation substring still
  matches after the typo fix), but both alter observable behaviour.
---

# Release 0.9.0 — correctness

> Fixes the three classes of defect found in the 2026-05-11 review:
> broken function references, user-visible typos, and a broken
> `getopts` loop. Sprint 1 issues only. No new features.

## Issues

| ID | Type | Title | Priority |
|---|---|---|---|
| [FEAT-200](bug/200-fix-undefined-command-accounts.md) | bug | Fix broken `command:accounts` / `add-account` references in `gpg-keys` and `sync` | **high** |
| [FEAT-201](bug/201-fix-user-visible-typos.md) | bug | Fix typos in user-visible strings (`syncronisation`, `unkown`, `paramenter`, etc.) | medium |
| [FEAT-202](bug/202-fix-getopts-loop.md) | bug | Fix broken `getopts` loop (manual shift, unhandled `-c:`) | medium |

## Dependencies

- No issues in this release depend on prior sprint work.
- FEAT-201 is a prerequisite for FEAT-206 (assertion tightening, 0.11.0).
- FEAT-202 is a prerequisite for FEAT-205 (flag-parsing tests, 0.11.0).

## Bats contract impact

| Change | Impact |
|---|---|
| FEAT-200 adds `command:accounts` fix | New tests added; no existing assertions change |
| FEAT-201 fixes typos in output strings | Existing test at line 94 still passes (substring `ynchronisation` ⊆ `synchronisation`); FEAT-206 later tightens it |
| FEAT-202 rewrites `getopts` loop | New flag tests added; no existing assertions change |

## Release checklist

- [ ] `bats tests/unit/secret.bats` — all 40 existing tests green
- [ ] New tests from FEAT-200 and FEAT-202 pass
- [ ] `shellcheck -S warning bin/secret` reports no new warnings
- [ ] `secret gpg-keys` with no args exits 0 without "command not found"
- [ ] `secret sync unreachable-account` exits 0 with a warn message
- [ ] `secret -q stores` and `secret -d version` behave correctly
- [ ] `.rpk/version` bumped to `0.9.0` and entry added to `.rpk/versions`
