---
id: FEAT-214
type: feature
priority: high
status: resolved
resolved-in: 0.11.0
---

# Reconcile PR state on every wake, not just on webhooks

## Description

**As a** maintainer relying on the agent to merge PRs as soon as
CI clears
**I want** the agent to reconcile every subscribed PR's state at
the start of each turn
**So that** a missed `check_run.completed` webhook does not leave
a green PR sitting at draft indefinitely.

`skills/automerging.md` §2b currently says: subscribe to PR
activity, end the turn, wait for the webhook. This assumes webhook
delivery is reliable. In practice it is not — events can be lost
when:

- The session is suspended at the moment the event fires.
- The MCP server hosting the subscription disconnects briefly.
- The webhook fires before the session is fully resumed.

Empirically observed on PR #2 in the 0.10.0 release: CI went
green at 17:52:03 but the session never received the wake event;
the PR sat as draft until the user prompted "why is the PR still
open?"

## Implementation

Add a new sub-section to `skills/automerging.md` §2b ("Reconcile
on every wake"):

```
The webhook is best-effort, the reconcile is not. At the start
of every response turn, if `subscribe_pr_activity` is active for
any PR, call `pull_request_read get_check_runs` and `pull_request_read
get` once per PR — before doing anything else. If the state has
changed since the last action (CI now green, mergeable_state now
clean, etc.), act on the new state immediately. This is a single
API call per PR per turn, not a poll.
```

Also update `skills/testing.md` §4 (the webhook table) to add a
trailing note:

```
Webhook delivery is best-effort. The agent reconciles state on
every wake — see skills/automerging.md §2b. CI failures that miss
the webhook still surface on the next turn.
```

## Acceptance Criteria

1. `skills/automerging.md` §2b contains an explicit "Reconcile on
   every wake" rule with the single-API-call-per-turn contract.
2. `skills/testing.md` §4 cross-references the reconcile rule.
3. A reader of either skill comes away with: "missed webhooks are
   tolerable; the agent picks the state up next turn".

## Notes

Discovered while shipping 0.10.0 (PR #2). The root cause analysis
session-conversation is the canonical record; this issue codifies
the fix.

The deeper fix is **layer 1**: enable `Settings → General → Pull
Requests → Allow auto-merge` so GitHub handles the merge itself
once required checks pass. Layer 1 is an admin toggle outside the
agent's control. FEAT-214 is the defence-in-depth layer that
handles the case where layer 1 is off (today's situation in
nostra124/secret).
