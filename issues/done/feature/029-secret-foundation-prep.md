---
id: FEAT-029
type: feature
priority: high
status: resolved
resolved-in: 0.9.0
resolved-by-audit: audits/audit-2026-05-13.md
---

# Make `secret` depend only on `account` and `config` at runtime

## Description

**As a** maintainer preparing to extract `secret` as its own rpk
package
**I want** `secret` to call only `account` and `config` (its
declared parents in the new dependency direction) at runtime
**So that** `secret` can be installed on top of the v0.13.0 / v0.14.0
foundation and every consumer (`bsync`, `container`, `crypt`,
`hosts`, `bitcoin` post-FEAT-010, …) gets a stable dependency.

Today `secret` calls: `account check config user`. There are no
cycles to break — `account` no longer calls `secret` after FEAT-022,
`config` no longer calls `secret` after FEAT-026, and `user` does
not call `secret` in either direction. So this prep is lighter than
the `account` or `config` versions: clean up the call set, no
back-edge surgery in sibling scripts.

After this ticket: `secret` calls `account` and `config` at runtime;
`rpk` is declared as deployment metadata only; `check` is inlined
where used; `user` is removed if not essential, kept (and declared
as a runtime dep) if it is.

## Implementation

1. **Audit every call site in `bin/secret`** that invokes another
   script and resolve each as follows:
   - `account` — kept (`secret → account`, runtime dep declared in
     the new repo).
   - `config` — kept (`secret → config`, runtime dep declared in
     the new repo).
   - `check` — inline whatever foundation logic `secret` uses from
     it.
   - `user` — review each call site; remove if `secret` can read
     `$USER` or get the equivalent via `account`. Keep and declare
     as a runtime dep if a removal would lose a real capability.
   - `scripts` — removed entirely by FEAT-001 (do `secret`'s
     subcommands here if FEAT-001 hasn't completed collection-wide).

2. **Verify** with
   `grep -wEn '(check|scripts|cache|data|hosts|repo)' bin/secret`
   that no script-call to a script *other than* `account`, `config`,
   and possibly `user` remains.

3. **Confirm no consumer creates a new cycle.** Existing callers of
   `secret` are: `account` (gone post-FEAT-022), `bsync`, `container`,
   `crypt`, `hosts`. None of those should be back-called from
   `secret` after this ticket.

4. **Add `docs/templates/CLAUDE.md.secret`** derived from
   `docs/templates/CLAUDE.md.foundation` (FEAT-022). Sections: scope
   (per-account secret storage), the no-shared-lib policy, what's
   intentionally duplicated and from where, who consumes this
   package.

## Acceptance Criteria

1. `grep -wEn '(check|scripts|cache|data|hosts|repo)' bin/secret`
   returns no script-invocation matches.
2. `bin/secret help` lists the same subcommands as before.
3. `bin/secret`'s remaining script calls are exactly: `account`,
   `config`, and (if kept) `user`.
4. No consumer of `secret` (`bsync`, `container`, `crypt`, `hosts`)
   gains a cycle as a result of this work.
5. `docs/templates/CLAUDE.md.secret` exists and references the
   foundation template.
6. Existing `secret` smoke tests (or a minimal suite added in this
   ticket if none exist) still pass after the refactor.
