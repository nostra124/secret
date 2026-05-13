---
id: FEAT-211
type: bug
priority: medium
status: resolved
resolved-in: 0.11.0
---

# `command:remotes` infinite-recurses when called with no args and no stores

## Description

**As a** user running `secret remotes` on a fresh install
**I want** the command to exit 0 with empty output
**So that** scripts wrapping `secret` don't hang.

`command:remotes` (`bin/secret:551-564`) calls itself with the
output of `command:stores` when invoked with no arguments. When
there are no stores, `command:stores` returns an empty string, so
`command:remotes` is called again with no arguments — and so on,
forever. This is the same pattern fixed in `command:gpg-keys` by
FEAT-200.

## Steps to reproduce

```bash
XDG_SECRET_STORES=$(mktemp -d) HOME=$(mktemp -d) ./bin/secret remotes
# (hangs — kill with Ctrl-C)
```

## Root cause

`bin/secret:551-564`:

```bash
command:remotes() {
    if [[ $# == 0 ]]; then
        command:remotes $(command:stores)   # ← empty when no stores
    else
        for STORE in $@; do
            ...
        done | sort -u
    fi
}
```

When `$(command:stores)` is empty, the recursive call carries no
arguments, takes the same branch, and recurses again. FEAT-200's
fix for `command:gpg-keys` introduced the guard pattern:

```bash
local STORES="$(command:stores)"
if [ -n "$STORES" ]; then
    command:remotes $STORES
fi
return 0
```

## Implementation

Apply the same guard pattern as `command:gpg-keys` (`bin/secret:715-724`).
Add a unit test that pins the no-args / no-stores case ("remotes
with no stores returns empty success" — already drafted in
FEAT-204 but deferred to this bug).

## Acceptance Criteria

1. `secret remotes` with no stores exits 0 with empty stdout
   within 1 second.
2. `secret remotes` with one or more stores still aggregates and
   `sort -u`'s the `git remote` output across stores (existing
   behaviour preserved).
3. A unit test pins the no-args / no-stores case.

## Notes

Discovered while writing FEAT-204 validation tests in 0.10.0 —
the test hung because of the infinite recursion. Assigned to
ROADMAP-0.11.0.md per `skills/milestones.md` (new bugs discovered
mid-milestone go to a future milestone, not the current one).
