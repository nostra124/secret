---
id: FEAT-216
type: bug
priority: high
status: resolved
resolved-in: 0.15.0
milestone: 0.15.0
---

# `password-store` `set`/`get` do not round-trip after `pass-init`

## Description

**As a** user who has run `secret pass-init` and stores entries in the
shared password-store
**I want** `secret set password-store/<name>` and `secret get
password-store/<name>` to round-trip the same value
**So that** the `pass(1)` integration (FEAT-041) is actually usable and
stays interoperable with the `pass` CLI.

A value written through `secret` was stored under a different name (or
not at all) than the one `secret get` / `pass show` read back, so the
round-trip never worked.

## Steps to reproduce

```bash
secret pass-init
printf 's3cr3t' | secret set password-store/email:work
secret get password-store/email:work   # ← error / empty
pass show email/work                    # ← "not in the password store"
```

## Root cause

Two independent defects, both carried verbatim from the shell version,
combined to make the password store unusable:

1. **Wrong `pass insert` entry name** (`lib/param.c`,
   `store_set_value`). The password-store branch invoked
   `pass insert -m <commit-message>`, passing the *commit message*
   (`"set <self> value <param>"`) as the `pass-name` positional
   argument. So `set` wrote to an entry literally named
   `set <self> value <param>` (and, because that string contains
   spaces, `pass` usually rejected it outright), while `get` read the
   entry `<param>`. Write and read addressed different names. It also
   never passed `--force`, so overwriting an existing entry was not
   idempotent.

2. **`pass-init` left the store unusable** (`lib/init.c`,
   `cmd_pass_init` and the `password-store` branch of `cmd_init`).
   `pass init` writes `.gpg-id`, but the follow-up `pass git init`
   commits through pass's own internal git, which fails silently when
   no git identity is configured (fresh container / CI, no global git
   config). With HEAD left unborn, the legacy `pass git reset --hard`
   then **wiped the uncommitted `.gpg-id`**, so every later
   `pass insert` / `pass show` failed with "you must run pass init".
   This is the same missing-git-identity class of bug fixed for `init`
   in the test-coverage work.

## Implementation

1. Added failing SIT tests first (`tests/sit/pass.bats`), per
   `skills/bugs.md`: a `set`→`get` round-trip that also asserts
   `pass show` agrees (interoperability), and an idempotent-overwrite
   case.
2. Fixed `store_set_value` to invoke `pass insert -m -f <param>` —
   the parameter name is the entry name, `--force` makes `set`
   idempotent, and stdin carries the value.
3. Replaced the fragile `pass git init` / `reset --hard` dance with a
   shared `pass_store_git_setup()` helper that git-inits the store,
   configures a per-repo `user.email`/`user.name` from the identity,
   and commits `.gpg-id` directly (no destructive reset). Used from
   both `cmd_pass_init` and the `password-store` branch of `cmd_init`.

## Acceptance Criteria

1. `secret set password-store/<name>` followed by `secret get
   password-store/<name>` returns the written value. ✓
2. The same entry is visible to the `pass` CLI at the equivalent path
   (`pass show <name>`). ✓
3. SIT tests reproduce the pre-fix failure and pass after the fix. ✓
4. Existing `tests/unit/secret.bats` contract is unchanged (147/147
   unit tests still pass — no major version bump required). ✓
