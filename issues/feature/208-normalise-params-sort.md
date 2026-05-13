---
id: FEAT-208
type: feature
priority: low
status: resolved
resolved-in: 0.11.0
---

# Normalise `sort -u` behaviour in `command:params` between password-store and regular stores

## Description

**As a** consumer of `secret params`
**I want** the output to be consistently sorted and deduplicated regardless of store type
**So that** scripts that parse the output (e.g. `bsync`, `container`) behave identically
for password-store and regular stores.

`command:params` at `bin/secret:184-203` has inconsistent behaviour:

- Regular store branch (`bin/secret:196-199`): output is sorted and deduplicated
  with `sort -u`.
- Password-store branch (`bin/secret:190-193`): no `sort -u`; output order is
  filesystem-dependent (`find * -type f`).

This means `secret params password-store` can return duplicates and unsorted output
while `secret params mystore` never can. Any consumer that relies on sorted output
will behave differently for the two store types.

## Implementation

1. Add `| sort -u` after the `sed` pipeline in the password-store branch of
   `command:params` (line 191), matching the regular-store branch.
2. Add a unit test that creates a password-store directory with multiple `.gpg` files
   and asserts that `secret params password-store` output is sorted:
   ```bash
   @test "params password-store output is sorted"
   ```

## Acceptance Criteria

1. `secret params password-store` output is sorted and deduplicated.
2. `secret params password-store` and `secret params <regularstore>` produce
   identical output format for equivalent contents.
3. New unit test passes.
4. Existing 40 unit tests still pass.
