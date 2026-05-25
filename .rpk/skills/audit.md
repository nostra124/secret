---
name: audit
description: |
  Run a traceability audit of `secret`. Trigger periodically
  (every milestone, at minimum), when behaviour seems undocumented,
  or when the user asks "is every subcommand covered by an issue"
  or "find orphan code". Cross-checks `bin/secret`, the test
  suites, the open issues (`issues/bug/`, `issues/feature/`), and
  the archived issues (`issues/done/`) — every behaviour the
  software exhibits must trace to at least one issue. Companion
  to `skills/milestones.md`, which establishes the invariant this
  skill enforces.
---

# `audit` skill — keep `issues/` honest

## 0. The invariant

> **Every behaviour the software exhibits is traceable to at
> least one open or done issue.** Every open or done issue
> describes behaviour that is either implemented (done), in
> progress (open + in-progress), or planned for a future
> milestone (open + listed in a `ROADMAP-<x.y.z>.md`).

The roadmap files (`issues/ROADMAP-<x.y.z>.md`) are the central
plan; the audit is what stops them from rotting. Without the
audit, `issues/` decays into yesterday's intent — a graveyard
that no longer maps to today's code.

## 1. When to run

| Trigger | Scope |
|---|---|
| End of every milestone (just before the retire commit) | Full audit |
| User reports unexpected behaviour | Targeted audit of the affected subcommand |
| Quarterly even without a release | Full audit |
| Before raising the version's major component | Full audit (the bats contract is changing — make sure every assertion still maps to an issue) |

The full audit is **not** optional at milestone boundaries; it
is part of the retire commit's checklist (`skills/milestones.md`
§5).

## 2. What the audit checks

Three directions of traceability:

### A. Code → issue

For every meaningful unit of behaviour in `bin/secret`, find the
issue that introduced or governs it.

| Code element | What to check |
|---|---|
| Each `command:<verb>` function | Has an introducing feature (`feature/`) and/or governing bug (`bug/`) |
| Each user-visible string (`error`, `warn`, `info`, `fatal`, help text) | Traces to an issue whose acceptance criteria reference the string |
| Each env-var (`SELF_DEBUG`, `SELF_QUIET`, `XDG_SECRET_STORES`, `SECRET_SKIP_TESTS`) | Traces to a feature or bug |
| Each external dependency (`account`, `gpg`, `pass`, `git`, `ssh`, `qrencode`) | Traces to a feature that introduced the integration |
| Each Makefile target | Traces to a feature (build-system features typically live as FEAT-19x per CLAUDE.md history) |
| Each `.github/workflows/*.yml` job | Traces to a feature (testing or CI feature) |

A code element with no matching issue is an **orphan**. Orphans
are filed as features (retroactively documenting existing
behaviour) and moved straight to `issues/done/feature/`.

### B. Issue → code

For every issue in `issues/done/bug/` or `issues/done/feature/`,
find the code (or test) that demonstrates it shipped.

| Issue element | What to check |
|---|---|
| Acceptance criterion #N | Maps to either a bats `@test` title, a code path in `bin/secret`, or a Makefile target |
| `resolved-in: <x.y.z>` | The release exists (`.rpk/versions` ledger entry) and the tag was pushed |
| Cross-references in the issue body | The referenced files / lines still exist (or the issue links to a follow-up that updates them) |

An issue marked `done` but with no demonstrable code change is a
**phantom**. Phantoms are either re-opened (the work didn't
actually happen) or annotated with the rationale (the change was
later reverted; cross-link the reverting issue).

### C. Test → issue

For every `@test` in `tests/unit/secret.bats` (and any SIT/PIT
files when those tiers exist), find the issue that owns it.

| Test element | What to check |
|---|---|
| `@test` title | Mentioned in at least one issue's acceptance criteria, implementation steps, or pinned-by clause |
| Test seeded with fixture data (`mkdir`, `touch` in `setup`) | The fixture pattern traces to the feature or bug being tested |

An untracked test is a **squatter**. Squatters are either
attached to an existing issue (file an amendment commit on the
issue's body) or split out as a follow-up feature documenting the
tested behaviour.

## 3. How to run

```sh
# Snapshot the inventory.
ls bin/                                # entry points
grep -n '^command:' bin/secret         # subcommand list
grep -nE 'error|warn|info|fatal' bin/secret | head -40
ls issues/bug/ issues/feature/ \
   issues/done/bug/ issues/done/feature/
grep -E '^@test' tests/unit/secret.bats

# Cross-reference, one direction at a time.
# A. Code → issue: for each subcommand, grep issues/ for FEAT-NNN
#    that mentions it.
for verb in $(grep -oP '^command:\K[^()]+' bin/secret); do
  hits=$(grep -rl "command:$verb\|\`$verb\`" issues/ 2>/dev/null)
  echo "$verb: $hits"
done | column -t

# B. Issue → code: for each done issue, grep bin/ + tests/ for the
#    acceptance-criteria keywords.
# (Manual reading of each issue.md file — script the keywords out
# of "Acceptance Criteria" sections and grep for them.)

# C. Test → issue: for each @test title, grep issues/ for it.
grep -oP '^@test "\K[^"]+' tests/unit/secret.bats | while read t; do
  hits=$(grep -rl "$t" issues/ 2>/dev/null)
  [ -z "$hits" ] && echo "SQUATTER: $t"
done
```

The commands above are starting points — they will produce
false-positive "orphan" / "squatter" / "phantom" flags that
require human judgement to resolve. The audit report should list
each flag with a one-line classification (real or false-positive)
and a follow-up action.

## 4. Outputs

The audit produces an `audit-<YYYYMMDD>.md` file (committed to
`issues/audits/`, not yet in the layout — create on first audit)
with these sections:

```
## Orphans (code with no issue)
- bin/secret:<L> — <element> — proposed FEAT-NNN: <title>

## Phantoms (done issues with no code)
- issues/done/<dir>/<NNN>-<slug>.md — <reason> — action: <reopen|annotate>

## Squatters (tests with no issue)
- tests/unit/secret.bats:<L> — <@test title> — action: <attach to FEAT-NNN|new feature>

## Bookkeeping gaps
- shipped milestone <x.y.z> not retired (ROADMAP file still present)
- issue resolved-in <x.y.z> but file still in issues/<dir>/ not issues/done/<dir>/

## Summary
- N orphans, M phantoms, K squatters, J bookkeeping gaps.
```

The audit file is itself a feature-issue-like artefact; commit
it on its own commit with subject "Audit <YYYY-MM-DD>: <N>
findings".

## 5. Acting on findings

| Finding | Action |
|---|---|
| **Orphan** (code, no issue) | File a retroactive feature documenting the behaviour. Move it straight to `issues/done/feature/`. The audit-discovery date doubles as the `resolved-in` (use the closest past release). |
| **Phantom** (done, no code) | Re-read the issue. If the work genuinely happened, annotate the issue with line/test references that prove it. If not, re-open: `git mv issues/done/<dir>/<NNN>-* issues/<dir>/` and shuffle into a future milestone (`skills/milestones.md` §4). |
| **Squatter** (test, no issue) | Either amend an existing issue's acceptance criteria to include the test title, or file a new feature that documents the tested behaviour. |
| **Bookkeeping gap** (shipped milestone not retired, etc.) | Run the `skills/milestones.md` §5 retire workflow now. |

Every finding ends with a commit. The audit is *only* complete
when every finding has either been resolved or annotated with a
reason it was kept open.

## 6. What is NOT in scope

- Code quality / style review. That is the standard PR review
  process (see `skills/automerging.md` operator's checklist), not
  the audit.
- Performance review. Out of scope for a per-script tool of
  `secret`'s size.
- Security review. Tracked through dedicated security issues
  (`type: bug`, `priority: high`) — not bundled into the audit.

The audit is narrow on purpose: it verifies traceability and
nothing else. A broader review is a separate effort.

## 7. Cross-references

- `skills/milestones.md` — establishes the traceability invariant
  this skill enforces.
- `skills/features.md`, `skills/bugs.md` — the issue formats the
  audit reads.
- `skills/version.md` §6 — the four-resolver consistency check
  the audit re-runs at every release boundary.
- `CLAUDE.md` §3 — issue-authoring rules; orphan retroactive
  features still follow them.
