---
version: 0.12.0
milestone: quoting
from: 0.11.0
status: planned
semver-justification: >
  Minor bump. FEAT-213 quotes every variable expansion in
  `bin/secret` and converts `for X in $(command:stores)` loops to
  `while IFS= read`. This is an internal refactor — no user-visible
  behaviour change on typical paths, but stores with spaces in
  their names will work after the change (instead of failing
  silently in most subcommands today).
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

- [ ] `shellcheck -S info -f gcc bin/secret` reports zero SC2086
- [ ] Every subcommand listed in FEAT-213's §Scope has a paired
      unit test that exercises it under a spaced `$SELF_CONFIG`
- [ ] `bats tests/unit/secret.bats` — all tests green
- [ ] `for STORE in $(command:stores)` patterns converted to
      `while IFS= read`
- [ ] `# shellcheck disable=SC2046` directive retained (or moved
      to per-line directives where possible)
- [ ] `VERSION` and `.rpk/version` bumped to `0.12.0`; ledger
      entry appended
