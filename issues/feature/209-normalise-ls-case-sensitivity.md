---
id: FEAT-209
type: feature
priority: low
status: open
---

# Normalise case handling between `stores` listing and `ls` prefix filtering

## Description

**As a** user running `secret ls`
**I want** the prefix filter to match stores regardless of the case used when
the store directory was created
**So that** `secret ls Alpha` and `secret ls alpha` return the same results when a
store named `Alpha` (or `alpha`) exists.

There is a case mismatch between two commands today:

- `command:stores` (`bin/secret:176-180`) lists directory names verbatim — so a
  store created as `Alpha/` is shown as `Alpha`.
- `command:ls` (`bin/secret:314-321`) lowercases the query prefix (`tr [A-Z] [a-z]`)
  before running `grep "^$QUERY"` against the output of `command:stores`. Since
  `command:stores` returns `Alpha` (not lowercased) and the query is lowercased to
  `alpha`, the grep never matches.

The inconsistency means `secret stores` can show a store that `secret ls <storename>`
cannot find.

## Options

**Option A — Lowercase store directory names at creation time** (`command:init`,
`command:set` etc.) — then `stores`, `ls`, `exists`, `params`, `has` all operate on
lowercase names, consistent with the `tr [A-Z] [a-z]` normalisation already applied
to arguments.

**Option B — Lowercase the `command:stores` output** in `command:ls` before grepping
— leaves disk names intact but gives case-insensitive lookup.

**Option C — Document the current behaviour** as intentional: store names are
case-sensitive, callers must use exact case. Remove the `tr` lowercasing from `ls`'s
prefix argument.

Option A is recommended: `bin/secret` already lowercases every user-supplied store
name before use (lines 185, 287, 301, 315 etc.), so the on-disk name should also be
lowercase to keep everything consistent.

## Implementation (Option A)

1. Ensure `command:init` (`bin/secret:221`) lowercases `$STORE` before calling
   `mkdir -p ${SELF_CONFIG}/${STORE}` — it already does (`tr [A-Z] [a-z]` at line
   222), so verify this is applied before the mkdir, not after.
2. Verify `command:set` lowercases the store component before creating the directory.
3. Add a unit test:
   ```bash
   @test "ls alpha finds store created as Alpha"
   ```
   which seeds `$SELF_CONFIG/Alpha/key.gpg`, then asserts `secret ls alpha` output
   contains `alpha/key` (or `Alpha/key`, depending on chosen option).

## Acceptance Criteria

1. The chosen option is recorded as a comment in `bin/secret` near `command:stores`.
2. `secret ls alpha` and `secret ls Alpha` return identical output.
3. `secret stores`, `secret ls`, and `secret exists` all agree on the visible name
   for a store regardless of how it was created.
4. New unit test passes.
5. Existing 40 unit tests still pass.
