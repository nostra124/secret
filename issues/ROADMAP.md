# `secret` — backlog roadmap

> Generated 2026-05-11 from code review of `bin/secret` and
> `tests/unit/secret.bats`. Issues are grouped by kind and ordered by
> recommended implementation sequence within each group. Bugs come
> before features at the same priority level (per CLAUDE.md §3).

---

## Bugs

| ID | Title | Priority | Blocks |
|---|---|---|---|
| [FEAT-200](bug/200-fix-undefined-command-accounts.md) | Fix broken `command:accounts` / `add-account` references in `gpg-keys` and `sync` | **high** | — |
| [FEAT-201](bug/201-fix-user-visible-typos.md) | Fix typos in user-visible strings (`syncronisation`, `unkown`, `paramenter`, etc.) | medium | FEAT-206 (test assertion) |
| [FEAT-202](bug/202-fix-getopts-loop.md) | Fix broken `getopts` loop (manual shift, unhandled `-c:`) | medium | FEAT-205 (flag tests) |
| [FEAT-203](bug/203-fix-unquoted-variable-expansions.md) | Quote variable expansions throughout `bin/secret` | medium | — |

---

## Test improvements

| ID | Title | Priority | Depends on |
|---|---|---|---|
| [FEAT-204](feature/204-test-missing-arg-validation.md) | Add arg-validation tests for `gen`, `def`, `ins`, `rem`, `clean`, GPG-key and `remotes` subcommands | **high** | — |
| [FEAT-205](feature/205-test-dispatcher-destroy-flags.md) | Add tests for `store/param` dispatcher shortcut, `destroy password-store`, and flag parsing | medium | FEAT-202 (flag tests) |
| [FEAT-206](feature/206-tighten-existing-test-assertions.md) | Tighten loose assertions: add status checks, verify version content, separate stderr | low | FEAT-201 (spelling fix) |
| [FEAT-207](feature/207-path-traversal-policy.md) | Define and enforce path-traversal policy for store and parameter names | medium | — |

---

## Code quality improvements

| ID | Title | Priority | Depends on |
|---|---|---|---|
| [FEAT-208](feature/208-normalise-params-sort.md) | Normalise `sort -u` in `command:params` between password-store and regular stores | low | — |
| [FEAT-209](feature/209-normalise-ls-case-sensitivity.md) | Normalise `ls` prefix case handling vs. `stores` listing | low | — |

---

## Recommended sequencing

```
Sprint 1 — correctness
  FEAT-200  (broken commands — high risk)
  FEAT-201  (typos — needed by FEAT-206)
  FEAT-202  (getopts — needed by FEAT-205 flag tests)

Sprint 2 — test coverage
  FEAT-204  (validation tests — independent, high value)
  FEAT-203  (quoting — shellcheck clean-up)
  FEAT-207  (path traversal policy decision + tests)

Sprint 3 — polish
  FEAT-205  (dispatcher / destroy / flag tests, after FEAT-202)
  FEAT-206  (assertion tightening, after FEAT-201)
  FEAT-208  (params sort normalisation)
  FEAT-209  (ls case normalisation)
```

---

## Notes

- FEAT-200 is the only **high-priority bug**: two documented subcommands (`gpg-keys`
  with no args, `sync`) call functions that do not exist. They need fixing before any
  release.
- FEAT-202 and FEAT-203 are prerequisites for the test improvements in FEAT-205 and
  for a clean `shellcheck` run.
- FEAT-207 requires a team decision (reject vs. document) before implementation; it
  is the only issue that needs discussion before coding can begin.
- FEAT-208 and FEAT-209 are cosmetic consistency fixes; defer if sprint capacity
  is tight.
- The CLAUDE.md major-version-bump rule applies to any change that alters the
  bats contract: adding new `fatal` messages or changing exit codes counts.
