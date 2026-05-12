---
id: FEAT-207
type: feature
priority: medium
status: resolved
resolved-in: 0.10.0
decision: Option A (reject) — chosen 2026-05-12 as the security-conscious default. `..` segments in store and parameter names now produce a fatal exit. The bats contract gains new "reject `..`" tests; existing assertions are unchanged.
---

# Define and enforce path-traversal policy for store and parameter names

## Description

**As a** security-conscious operator
**I want** `secret` to either reject or document its handling of `..` in store and
parameter names
**So that** an attacker or misconfigured caller cannot read or write files outside the
store directory.

Currently `secret` lowercases input and replaces `:` with `/`, but does not strip or
reject `..` segments. For example:

```bash
secret get ../etc/passwd          # STORE="..", PARAM="etc/passwd"
secret exists ../../root/.ssh     # lowercases, checks $SELF_CONFIG/../../root/.ssh
secret set ../shadow/root <value> # could write outside the store root
```

`SELF_CONFIG` expansion without `..`-stripping means a caller can address any
directory reachable from `$SELF_CONFIG` by walking upward. Whether this is
exploitable depends on deployment (single-user workstation vs. shared system with
root-mode `secret`), but the policy should be explicit either way.

## Options

**Option A — Reject**: add a guard at the top of every store/param parser:
```bash
[[ "$STORE" == *".."* ]] && fatal "store name may not contain '..'"
[[ "$PARAM" == *".."* ]] && fatal "parameter name may not contain '..'"
```

**Option B — Document**: add a note to `docs/secret.md` (and the man page per
FEAT-042) stating that store and parameter names are caller-controlled and the
caller is responsible for not supplying `..` segments. Add a test that asserts the
*current* (permissive) behaviour so any accidental tightening is visible.

The recommended option is **A** for any deployment where `secret` may be called
with `EUID=0` (the root branch at `bin/secret:70-74` makes this a real path).

## Implementation

1. Decide on Option A or B via a team discussion; record the decision in this issue
   before implementation.
2. If Option A: add `..`-rejection guards in `command:exists`, `command:has`,
   `command:get`, `command:set`, `command:del`, `command:params`, `command:destroy`,
   `command:init`. Centralise the guard in a helper `validate_name()` function.
3. Add unit tests regardless of option:
   - `secret exists ../foo` — assert exit non-zero and `fatal` message (Option A) or
     exit non-zero for a different reason (Option B, document which).
   - `secret get ../etc/passwd` — same.
   - `secret set ../shadow/root` — same.
4. Update `docs/secret.md` and the man page (FEAT-042 scope) with the policy.

## Acceptance Criteria

1. A written policy (code comment, doc, or this issue) describes what `secret` does
   with `..` in names.
2. At least three unit tests cover `..`-containing inputs and assert the documented
   behaviour.
3. If Option A: `shellcheck` does not warn about the guard expressions.
4. If Option A: the guard is applied consistently to all subcommands that parse a
   store or parameter name.
5. Existing 40 unit tests still pass.
