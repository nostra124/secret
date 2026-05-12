---
id: FEAT-210
type: bug
priority: medium
status: open
---

# `command:clean` is advertised in help but not implemented

## Description

**As a** user reading `secret help`
**I want** every advertised subcommand to actually run
**So that** the help text is a reliable contract.

`secret help` lists `clean <store>` under "store management commands"
(`bin/secret:134`), but there is no `command:clean` function defined
in `bin/secret`. Invoking `secret clean somestore` falls through to
the dispatcher's "unknown command" fatal exit — the help advertises
behaviour that does not exist.

## Steps to reproduce

```bash
./bin/secret help | grep -A0 '^   clean'
#   clean <store>                 cleans empty parameters in store <store>

./bin/secret clean somestore
# secret: fatal - unknown command clean
# exit code: 255
```

## Root cause

`bin/secret:134` advertises `clean`. No `command:clean` exists
(verified via `grep -n '^command:' bin/secret`). The dispatcher at
`bin/secret:825-830` only routes to defined functions, so the help
text and the implementation have drifted.

## Implementation

Two valid resolutions; pick one before coding:

**Option A — Implement `command:clean`** to match the help text. The
described behaviour is "cleans empty parameters in store <store>",
i.e. remove zero-byte `.gpg` files inside `$SELF_CONFIG/<store>/`:

```bash
command:clean() {
    local STORE=$(echo "$1" | tr '[A-Z]' '[a-z]')
    test -z "$STORE" && fatal "please specify a store"
    [ -d "${SELF_CONFIG}/${STORE}" ] || fatal "store ${STORE} does not exist"
    find "${SELF_CONFIG}/${STORE}" -type f -size 0 -name '*.gpg' -delete
}
```

**Option B — Remove the line from `command:help`** since nobody has
asked for the feature. Cheaper but drops a documented capability.

## Acceptance Criteria

1. `secret clean` exits non-zero with a "please specify a store"
   fatal message (Option A) or no longer appears in `secret help`
   (Option B).
2. `secret clean nope` exits non-zero with "store nope does not
   exist" (Option A) or "unknown command clean" (Option B).
3. Option A only: a unit test in `tests/unit/secret.bats` seeds
   a store with one empty and one non-empty `.gpg` file, runs
   `secret clean`, and asserts only the empty file was removed.
4. FEAT-204's deferred clean-tests can be enabled or removed once
   this issue ships.

## Notes

Discovered while writing FEAT-204 validation tests in 0.10.0.
Assigned to ROADMAP-0.11.0.md per skills/milestones.md (new bugs
discovered mid-milestone go to a future milestone, not the current
one).
