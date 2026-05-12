---
name: automerging
description: |
  Auto-merge a PR for `secret` once CI is green. Trigger when a PR
  has been opened, the changes are reviewed (or trivial), and the
  reviewer/owner wants the merge to fire automatically the moment
  the CI gate clears — without polling. Covers when auto-merge is
  appropriate, the GitHub mechanics (`merge_method`, ready-for-
  review state, branch protection), and the failure-to-bug
  pipeline that activates when CI fails instead of passing.
---

# `automerging` skill — let GitHub merge when CI is green

## 0. The policy

> **Every PR that has been reviewed and whose tests pass should
> be merged the moment CI clears.** GitHub auto-merge is the
> preferred mechanism — but when it is disabled at the repo level,
> the agent (or maintainer) merges manually as soon as the green
> check arrives. Either way, no PR sits with a green check and
> waits.

Auto-merge collapses the wait for CI into a single decision: "if
CI passes, merge". It does not bypass any required check — it
just trusts them.

Manual merging is reserved for two situations: (a) the repo has
not enabled auto-merge in `Settings → General → Pull Requests`, in
which case the agent merges via `mcp__github__merge_pull_request`
the moment CI is green; (b) a coordinated release where multiple
PRs must land in a specific order.

If CI fails, **neither auto-merge nor the manual fallback fires**.
The failure becomes a bug (see §4 and `skills/bugs.md`), and the
PR stays open until green.

## 1. Prerequisites

For auto-merge to actually fire, all of the following must hold:

1. **Repo-level toggle is on**: `Settings → General → Pull
   Requests → Allow auto-merge`. If this is off,
   `mcp__github__enable_pr_auto_merge` fails with
   _"Auto-merge is not enabled for this repository"_. Fall back
   to the manual merge path in §2a.
2. The PR is **not a draft**. Mark it ready-for-review first
   (see §2).
3. The base branch has **at least one required status check**
   configured under branch protection (see `skills/testing.md` §4).
   Auto-merge needs something to wait for.
4. The PR has **no unresolved review threads** (if branch
   protection requires approving reviews).
5. The branch is **mergeable** (no conflicts; `mergeable_state`
   is `clean` or `unstable`, not `dirty`).

If any of these are missing, enabling auto-merge returns an error
or silently does nothing. Check before calling the API.

## 2a. Manual merge — the fallback path

When prerequisite §1 is unmet (repo-level auto-merge disabled),
do this instead of polling or waiting:

```text
# 1. Read CI state.
pull_request_read get_check_runs
  → every check has status: completed and conclusion: success

# 2. Read PR state.
pull_request_read get
  → mergeable_state: clean, draft: false

# 3. Merge immediately.
merge_pull_request merge_method: squash
```

The decision rule is identical to auto-merge — "if CI is green
and the PR is mergeable, merge" — only the execution differs.
The agent invokes the merge directly via
`mcp__github__merge_pull_request` rather than asking GitHub to do
it on the next webhook.

The fallback is **not** an excuse to skip CI. If CI is pending or
red, do not merge by hand. Wait for green (subscribe to the PR
webhook via `subscribe_pr_activity` rather than polling), or file
a bug if CI is red.

A reviewer who wants the repo-level toggle enabled — and so the
agent stops doing manual merges — flips
`Settings → General → Pull Requests → Allow auto-merge` once;
subsequent PRs go through the full auto-merge path.

## 2. Mechanics — GitHub MCP

The MCP tools used here:

| Tool | Purpose |
|---|---|
| `mcp__github__pull_request_read` (method `get`) | Inspect mergeable state, draft status, mergeable_state |
| `mcp__github__pull_request_read` (method `get_check_runs`) | Confirm CI is green or pending |
| `mcp__github__update_pull_request` | Set `draft: false` to mark ready-for-review |
| `mcp__github__enable_pr_auto_merge` | Turn on auto-merge with a chosen merge method |
| `mcp__github__disable_pr_auto_merge` | Turn it off (e.g. before force-pushing a fix) |
| `mcp__github__merge_pull_request` | Manual fallback when repo-level auto-merge is off (see §2a) |

Typical invocation:

```text
# 1. Read state.
pull_request_read get → mergeable_state: clean, draft: true
pull_request_read get_check_runs → success or pending

# 2. Mark ready (only if currently draft).
update_pull_request draft: false

# 3. Enable auto-merge.
enable_pr_auto_merge merge_method: squash
```

`merge_method` choices: `merge` (default), `squash`, `rebase`.
Pick **`squash`** for feature PRs (linear master history,
WIP commits collapsed) and **`merge`** only when the commit
history of the PR matters (rare for `secret`'s small surface).

## 3. When NOT to auto-merge

| Situation | Reason |
|---|---|
| The PR breaks the bats contract (intentional behaviour change) | Needs explicit review of the version bump in the same PR |
| The PR introduces a new test tier (SIT/PIT) | Requires reviewer to verify the runner script actually exercises something |
| The PR touches `.github/workflows/` | A bad CI change can disable the gate it's supposed to enforce; review by hand |
| The PR is mid-stack of a coordinated multi-PR release | Manual ordering matters; auto-merge could land them out of order |

In all four cases, leave auto-merge off and merge by hand after
the reviewer signs off.

## 4. CI failure → bug pipeline

When CI fails on an auto-merge-enabled PR:

1. The workflow posts the trimmed failure log as a PR comment
   (`skills/testing.md` §4).
2. Auto-merge stays armed but **does not fire** — it waits for the
   next push.
3. Open a bug issue (`skills/bugs.md`) using the CI comment as the
   reproduction record.
4. Follow the TDD loop in `skills/bugs.md`:
   - Reproduce locally.
   - Write a failing test that captures the CI defect.
   - Implement the fix.
   - Push.
5. The next CI run flips green → auto-merge fires → done.

Auto-merge effectively means "no human in the loop on the happy
path, but a clear failure-to-bug path when it goes wrong." This
is only safe because the bug pipeline is strict — never disable
auto-merge to "merge anyway"; fix the bug instead.

## 5. Operator's checklist

Before enabling auto-merge on a PR:

- [ ] PR description is accurate and includes a test plan.
- [ ] Issue file(s) referenced in the PR have `status:
      in-progress`; will be flipped to `status: resolved` in the
      merge commit (or a follow-up).
- [ ] Roadmap (`issues/ROADMAP-<x.y.z>.md`) for the affected
      version has matching checkboxes ticked.
- [ ] `VERSION` and `.rpk/version` agree (`skills/version.md` §6).
- [ ] Reviewer (if separate from the author) has approved or the
      PR is trivial enough for owner-merge.
- [ ] No `wip:` or `draft:` markers remain in commit messages.
- [ ] PR is **not** marked draft.

Only after all eight items pass: enable auto-merge.

## 6. Disabling auto-merge

Reasons to call `mcp__github__disable_pr_auto_merge`:

- A force-push is needed to amend history (auto-merge would race).
- The reviewer wants to land the PR manually after all.
- A coordinated release demands a specific merge order.

Disable, perform the action, then re-enable if appropriate.

## 7. Cross-references

- `skills/testing.md` — how CI is configured and what it gates.
- `skills/bugs.md` — the workflow when CI fails instead of passing.
- `skills/features.md` — feature-issue authoring; auto-merge is
  step 4 of "from issue to landed code".
- `skills/version.md` — releases that go through `make package`
  are merged by the package author by hand, not auto-merge.
