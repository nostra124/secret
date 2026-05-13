---
id: FEAT-212
type: bug
priority: medium
status: open
---

# `command:ins` and `command:rem` mask all error exits

## Description

**As a** caller of `secret ins` or `secret rem` who passes a bad
argument
**I want** a non-zero exit so my wrapper script can branch on the
failure
**So that** I find out about bad inputs at the point of failure
rather than silently corrupting downstream state.

Both `command:ins` (`bin/secret:532-538`) and `command:rem`
(`bin/secret:541-548`) end with an unconditional `return 0`, so
they always succeed from the caller's point of view — even when
the underlying `command:get` / `command:set` they delegate to
print fatal messages to stderr and abort their subshells.

## Steps to reproduce

```bash
echo "x" | XDG_SECRET_STORES=$(mktemp -d) HOME=$(mktemp -d) \
  ./bin/secret ins; echo "exit=$?"
# secret: fatal - please specify a parameter
# secret: fatal - please specify a parameter
# exit=0           ← should be non-zero
```

## Root cause

`bin/secret:532-538`:

```bash
command:ins() {
    local PARAM=$1
    INS_VALUE=$(cat)
    OLD_VALUE=$(command:get $PARAM)
    echo -e "${INS_VALUE}\n${OLD_VALUE}" | sort -u | grep "\S" | command:set $PARAM
    return 0   # ← swallows every error from get/set/pipeline
}
```

`command:get` and `command:set` print fatal stderr lines and
`exit -1` when called with empty `$PARAM`, but those exits only
kill the `$(...)` subshell. The outer `command:ins` continues,
runs the pipeline (which also fails), then returns 0.

`command:rem` has the same shape (`bin/secret:541-548`).

## Implementation

Two fixes, both small:

1. Hoist the argument-validation guard from `command:get` /
   `command:set` into `command:ins` and `command:rem` directly,
   so the fatal fires before any subshell. Pattern:

   ```bash
   command:ins() {
       local PARAM=$1
       test -z "$PARAM" && fatal "please specify a parameter"
       [[ "$PARAM" == */* ]] || fatal "please specify in the format <store>/<param>"
       ...
   }
   ```

2. Remove the unconditional `return 0`. Let the pipeline's exit
   status propagate via `set -o pipefail` (declared at the top of
   `bin/secret`, currently absent — adding it is a separate
   concern, track if needed).

## Acceptance Criteria

1. `echo | secret ins` exits non-zero with "please specify a
   parameter".
2. `echo | secret rem` exits non-zero with "please specify a
   parameter".
3. `echo "y" | secret ins storealone` exits non-zero with the
   format-error fatal.
4. Two unit tests pin both subcommands' missing-arg and bad-arg
   exits (the ones deferred from FEAT-204).
5. Happy-path SIT tests (when added) still pass — the masking
   only hid validation failures, not real workflow.

## Notes

Discovered while writing FEAT-204 validation tests in 0.10.0.
Assigned to ROADMAP-0.11.0.md per `skills/milestones.md`. The
two corresponding FEAT-204 tests ("ins without arg exits
non-zero", "rem without arg exits non-zero") are deferred to
this issue.
