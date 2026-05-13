---
id: FEAT-213
type: feature
priority: low
status: resolved
resolved-in: 0.12.0
---

# Deep SC2086 cleanup — quote all variable expansions in `bin/secret`

## Description

**As a** user with non-standard paths (spaces, special characters)
or a security-conscious operator
**I want** every variable expansion in `bin/secret` to be properly
quoted
**So that** paths containing whitespace work everywhere and shell
metacharacters in store/parameter names cannot accidentally be
re-interpreted.

FEAT-203 (shipped in 0.10.0) removed the `eval` calls and quoted
the most user-facing path (`command:stores`) plus a few helpers.
It also added a file-level `# shellcheck disable=SC2046` directive
documenting that command-substitution word splitting is intentional
in this codebase (multi-recipient gpg-encrypt, multi-store remotes,
multi-file git checkout).

What remains is **225 SC2086 (info-level) warnings** — every other
`${SELF_CONFIG}`, `${STORE}`, `$PARAM`, etc. expansion that isn't
yet quoted. None block today's tests (the unit suite happens to
seed paths without spaces), but a store with a space in its name
fails in every subcommand except `command:stores`.

## Scope

Each of the following functions has at least one unquoted path
reference; FEAT-213 quotes them all and adds a corresponding bats
test that exercises the function under a spaced `$SELF_CONFIG`:

- `command:params`
- `command:init`
- `command:exists`
- `command:destroy`
- `command:ls`
- `command:has`
- `command:set`
- `command:get`
- `command:del`
- `command:qr`
- `command:gen`
- `command:def`
- `command:edit`
- `command:remotes`
- `command:pull`
- `command:push`
- `command:sync`
- `command:gpg-keys`
- `command:add-gpg-key`
- `command:del-gpg-key`
- `command:has-gpg-key`

Plus `for STORE in $(command:stores)` loops — those iterate via
word splitting and would break with spaces in store names. Each
such loop becomes:

```bash
while IFS= read -r STORE; do
    ...
done < <(command:stores)
```

## Implementation

1. Run `shellcheck -S info -f gcc bin/secret | grep SC2086` and
   work through the list line-by-line; quote every variable
   reference that survives the `# shellcheck disable=SC2046`
   directive at the top of the file.
2. Convert `for X in $(command:stores)` patterns to `while IFS= read`
   (about 6 sites in the script).
3. For each function quoted, add a unit test under
   `tests/unit/secret.bats` that creates `$SELF_CONFIG` with a
   space in the path (via `XDG_SECRET_STORES`) and verifies the
   function behaves correctly.
4. Re-run `shellcheck -S info` and confirm SC2086 count is zero.

## Acceptance Criteria

1. `shellcheck -S info -f gcc bin/secret` reports zero SC2086.
2. Every subcommand listed in §Scope has a paired unit test that
   exercises it under a spaced `$SELF_CONFIG`.
3. Total `bats tests/unit/secret.bats` count grows by at least one
   per subcommand listed in §Scope.
4. Existing tests still pass.
5. The file-level `# shellcheck disable=SC2046` directive remains
   (or moves to per-line directives if some sites can be quoted).

## Notes

Filed during FEAT-203 work in 0.10.0. The 225-instance scope was
too large to absorb into 0.10.0 without slipping the milestone;
splitting it out into FEAT-213 keeps the per-session model intact
(see `skills/milestones.md` §3).

Assigned to ROADMAP-0.11.0.md.
