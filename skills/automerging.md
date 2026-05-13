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
red, do not merge by hand. Wait for the next webhook event (see
§2b), or file a bug if CI is red.

A reviewer who wants the repo-level toggle enabled — and so the
agent stops doing manual merges — flips
`Settings → General → Pull Requests → Allow auto-merge` once;
subsequent PRs go through the full auto-merge path.

## 2b. Listening for CI completion — never poll

> **The agent MUST subscribe to PR activity webhooks and react to
> events. Polling, sleeping, or otherwise blocking the session
> while CI runs is forbidden — it hangs the session for the
> minutes-to-hours it takes CI to finish.**

Webhook events are delivered as `<github-webhook-activity>`
messages that wake the session. Subscribe once per PR:

```text
mcp__github__subscribe_pr_activity pullNumber: <N>
```

Once subscribed, the agent receives **every** CI state transition:

| Event | What the agent does |
|---|---|
| `check_run` `completed` with `conclusion: success` | Re-read prerequisites; if §1 is met enable auto-merge, otherwise execute the §2a manual fallback |
| `check_run` `completed` with `conclusion: failure` | Open a bug per `skills/bugs.md`; do not merge |
| `check_run` `completed` with `conclusion: cancelled` / `timed_out` / `action_required` | Treat as failure: investigate, file a bug if the cause is in the code, re-trigger CI if it's a flake |
| `pull_request_review` `submitted` | If approving review unblocks merge, re-check prerequisites |
| `issue_comment` / `pull_request_review_comment` | Read; act if actionable (per `skills/bugs.md`) |

After enabling subscription, **end the turn**. The webhook will
wake the session when there is something to do. Do not write
`Bash sleep`, do not repeatedly call `pull_request_read
get_check_runs`, do not chain status checks in a `while` loop —
those patterns are explicitly prohibited and waste tokens while
producing nothing.

Two corollaries:

1. The agent must distinguish **green** events (act: merge) from
   **red** events (act: open a bug). Both arrive through the same
   webhook stream; the `conclusion` field on the `check_run`
   event is the discriminator.
2. A subscription persists for the life of the PR; it survives
   the session being suspended and resumed. No need to
   re-subscribe on every commit push.

## 2c. Reconcile on every wake — the webhook is best-effort

> **At the start of every response turn, if `subscribe_pr_activity`
> is active for any PR, call `pull_request_read get_check_runs`
> and `pull_request_read get` once per PR before doing anything
> else. If state has changed since the last action — CI now
> green, `mergeable_state` now `clean`, a new review thread, etc.
> — act on the new state immediately.**

Webhook delivery is best-effort, the reconcile is not. Events can
be lost when:

- The session is suspended at the moment the event fires.
- The MCP server hosting the subscription disconnects briefly.
- The webhook fires before the session is fully resumed.

Empirically observed on `nostra124/secret#2`: CI went green but
the `check_run.completed` event never reached the session; the
PR sat as draft until the user prompted "why is the PR still
open?" The fix is one API call per PR per turn — bounded, cheap,
and immune to webhook drop.

This is **not** polling. Polling is "check repeatedly while
nothing changes". Reconciling is "check once when something
*might* have changed — a new turn starts". The two are
operationally identical (one API call), but reconciling is
guaranteed to terminate (one call per turn, period), while
polling has no upper bound.

The deeper fix is **layer 1** in §0 (repo-level
`Allow auto-merge`). Reconciling is the defence-in-depth that
keeps things moving until layer 1 is on.

## 2. Mechanics — GitHub MCP

The MCP tools used here:

| Tool | Purpose |
|---|---|
| `mcp__github__subscribe_pr_activity` | Subscribe to PR webhooks (mandatory — see §2b) |
| `mcp__github__unsubscribe_pr_activity` | Stop receiving events for a PR |
| `mcp__github__pull_request_read` (method `get`) | Inspect mergeable state, draft status, mergeable_state |
| `mcp__github__pull_request_read` (method `get_check_runs`) | Confirm CI is green or pending (called once per webhook event — never in a loop) |
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
