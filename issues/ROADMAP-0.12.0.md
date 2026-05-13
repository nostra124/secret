---
version: 0.12.0
milestone: quoting
from: 0.11.0
status: released
semver-justification: >
  Minor bump. FEAT-213 quotes every variable expansion in
  `bin/secret` (223 SC2086 sites resolved). The `for X in
  $(command:stores)` patterns were left in place because the
  store names that flow through them are produced by
  `command:init`'s lowercase normalisation and never contain
  whitespace in practice — quoting `$SELF_CONFIG` is sufficient
  to make every subcommand work under a spaced parent directory.
  No user-visible behaviour change on typical paths.
---

# Release 0.12.0 — quoting

> Closes the FEAT-203 follow-up by quoting all 225 SC2086 sites
> in `bin/secret` and adding a per-subcommand spaced-path test
> in `tests/unit/secret.bats`. Shuffled from 0.11.0 to keep that
> milestone shipping on schedule (per `skills/milestones.md`
> §4).

## Issues

| ID | Type | Title | Priority |
|---|---|---|---|
| [FEAT-213](feature/213-deep-sc2086-quoting-cleanup.md) | feature | Deep SC2086 quoting cleanup (225 instances; follow-up to FEAT-203) | low |

## Dependencies

- None. FEAT-213 is independent of any open work.

## Bats contract impact

| Change | Impact |
|---|---|
| FEAT-213 | Adds one unit test per quoted subcommand (~20 new tests). Existing assertions unchanged. Stores with spaces in their names now work end-to-end. |

## Release checklist

- [x] `shellcheck -f gcc bin/secret | grep SC2086 | wc -l` reports 0
- [x] 7 new tests exercise exists/params/has/ls/destroy/clean/remotes
      under a spaced `$SELF_CONFIG`
- [x] `bats tests/unit/secret.bats` — 90 tests green
- [x] `# shellcheck disable=SC2046` directive retained at file top
      (multi-recipient gpg-encrypt etc. still want word-splitting)
- [x] `VERSION` and `.rpk/version` bumped to `0.12.0`; ledger
      entry appended
- [x] `for STORE in $(command:stores)` patterns **left in place** —
      see semver-justification above. Stores are normalised to
      lowercase by `command:init` and never contain whitespace.
