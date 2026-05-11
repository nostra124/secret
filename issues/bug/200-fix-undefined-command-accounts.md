---
id: FEAT-200
type: bug
priority: high
status: open
---

# Fix broken `command:accounts` and `add-account` references in `gpg-keys` and `sync`

## Description

**As a** user running `secret gpg-keys` with no arguments or `secret sync`
**I want** the command to either work or fail with a clear error message
**So that** I can trust that every documented subcommand is at least minimally functional.

`command:gpg-keys` (with no store argument, `bin/secret:711`) calls `command:accounts`,
which does not exist anywhere in `bin/secret`. Likewise `command:sync` (`bin/secret:691`)
calls `add-account` before the loop over stores, and references `$STORE` before it is set.
Both code paths silently invoke nonexistent functions, producing confusing output or
silent no-ops depending on the shell environment.

## Steps to reproduce

```bash
secret gpg-keys          # calls command:accounts — not defined
secret sync someaccount  # calls add-account $STORE $ACCOUNT — $STORE undefined, add-account missing
```

## Root cause

`command:gpg-keys` line 711:
```bash
command:accounts $(command:stores)
```
`command:accounts` was never defined in `bin/secret`; likely a leftover rename from an
earlier version where the function was called something else.

`command:sync` line 691:
```bash
add-account $STORE $ACCOUNT
```
`$STORE` has not been set at that point in the function, and `add-account` is not
defined anywhere in the script.

## Implementation

1. In `command:gpg-keys` (no-store branch, line 710-713): replace
   `command:accounts $(command:stores)` with `command:gpg-keys $(command:stores)` — the
   intent is to recurse over all stores and collect their keys.
2. In `command:sync` (line 691): determine intended semantics of `add-account`. If it
   should add the remote to every store, replace with an inline loop calling
   `git remote add` inside each store directory (mirroring the pattern in `command:pull`
   and `command:push`). Remove the reference to the undefined `$STORE`.
3. Add unit tests in `tests/unit/secret.bats` for both code paths up to the first
   external dependency (network / `account online`).

## Acceptance Criteria

1. `secret gpg-keys` with no args exits 0 and produces no "command not found" output.
2. `secret sync` with an unreachable account exits 0 with a warn message (matching the
   existing warn path at line 700), not a function-not-found error.
3. `shellcheck -S warning bin/secret` reports no undefined-function warnings for these
   two call sites.
4. Unit tests cover both the no-store branch of `gpg-keys` and the no-account branch
   of `sync`.
