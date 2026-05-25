---
name: features
description: |
  Author a feature for `secret`. Trigger when proposing a new
  capability, sub-command, or enhancement that does not fit the
  bug category. Covers the FEAT-NNN frontmatter, the user-story
  body, acceptance-criteria conventions, and how features land on
  a roadmap. Bugs go through `skills/bugs.md` instead — features
  and bugs share the FEAT-NNN pool but use different priority
  rules and a different workflow.
---

# `features` skill — author a feature issue

## 0. Feature vs. bug

| You are about to log… | Process |
|---|---|
| The software behaves incorrectly against its documented contract or a sensible expectation | `skills/bugs.md` (TDD required) |
| The software does not yet do something, and you want it to | this skill |

A misbehaviour discovered while *building a feature* is still a
bug — file it separately under `skills/bugs.md` so the TDD loop
runs against it. Don't fold "and while we're at it, fix X" into a
feature PR.

## 1. Issue file format

Features live under `issues/feature/<NNN>-<slug>.md` and use the
same `FEAT-NNN` numbering pool as bugs (CLAUDE.md §3). Frontmatter:

```yaml
---
id: FEAT-NNN
type: feature
priority: high | medium | low
status: open | in-progress | resolved
resolved-in: <x.y.z>        # added when the feature ships
---
```

`type: feature` is the only difference from a bug's frontmatter
(`type: bug`). Sequencing rules differ — see §3.

## 2. Mandatory body sections

Every feature file has these sections in this order:

1. **Title** (`# <Imperative description of the new capability>`)
2. **Description** — a `As a / I want / So that` user story plus
   the surrounding context (which subsystems it touches, which
   FEAT-NNNs it relates to).
3. **Implementation** — a numbered list of steps describing *what
   the code change looks like*, not just the outcome. Reference
   line numbers in `bin/secret` if helpful.
4. **Acceptance Criteria** — a numbered list of observable,
   testable assertions that mark the feature done. Every item
   should be verifiable with a single command or test.

Optional sections:

- **Options** (when there is a policy decision the team must make
  before coding — see `issues/feature/207-path-traversal-policy.md`
  for the pattern).
- **Test plan** — when the feature touches multiple tiers
  (unit / SIT / PIT).

Pattern examples in the tree: `issues/feature/204-*.md`,
`issues/feature/205-*.md`, `issues/feature/207-*.md`.

## 3. Priority sequencing rules

Within a sprint (`issues/ROADMAP-<x.y.z>.md`), features are
ordered:

1. **Bugs first** — bugs of any priority precede features of
   the same priority (CLAUDE.md §3).
2. Features ordered by priority: high → medium → low.
3. Within a priority, dependencies first: a feature whose
   acceptance depends on another must come second.

A `high`-priority feature can still ship in a sprint that
contains `medium` bugs — but a `medium` bug ships before a
`medium` feature.

## 4. Acceptance criteria — what makes a good one

A good acceptance criterion is:

- **Observable** — runnable as a command, or visible as output of
  one (`secret stores`, `shellcheck`, `bats`).
- **Specific** — names the exact file, function, or output string.
- **Atomic** — one criterion per checkbox; don't bundle.

Good:

```
1. `bats tests/unit/secret.bats` includes a test named
   `gen without arg exits non-zero` and it passes.
2. `shellcheck -S warning bin/secret` reports zero SC2086
   warnings.
3. `.rpk/version` and `VERSION` both contain the same string.
```

Bad:

```
1. Everything works.
2. The code is cleaner.
3. Tests pass.
```

## 5. From issue to landed code

```
1. Author the issue file
   - issues/feature/<NNN>-<slug>.md with type: feature, status: open.
   - Insert into the next ROADMAP-<x.y.z>.md sprint, respecting
     bug-before-feature ordering.

2. Implement
   - Add tests under tests/unit/ (and SIT/PIT if appropriate).
     Tests do NOT need to precede the implementation for features
     (unlike bugs) — write them alongside.
   - Make the code change.
   - Run `make check-unit` (or `check-all` if you touched
     SIT/PIT-relevant paths) — green before commit.

3. Commit and push
   - Reference the FEAT-NNN in the commit subject.
   - Update issue frontmatter: status: in-progress on first push,
     status: resolved + resolved-in: <x.y.z> on the merge commit.

4. CI + auto-merge
   - The PR runs the bats suite in CI (skills/testing.md §4).
   - On green, auto-merge fires (skills/automerging.md).
   - On red, the failure log arrives as a PR comment — treat that
     as a bug and switch to skills/bugs.md.
```

## 6. Cross-references

- `skills/bugs.md` — bug authoring with the mandatory TDD loop.
- `skills/testing.md` — where the tests run and what gates merging.
- `skills/version.md` — when a feature triggers a version bump.
- `skills/automerging.md` — what happens after CI goes green.
- `CLAUDE.md` §3 — top-level issue-authoring rules.
- `CLAUDE.md` §8 — semver decision rules (a new sub-command is at
  least a minor bump; a breaking change to an existing sub-command
  contract is major).

## 7. Common anti-patterns

- **Feature creep inside a single issue.** If two acceptance
  criteria describe orthogonal capabilities, split into two
  features.
- **Skipping the user story.** "Add `secret xyz`" with no `As a / I
  want / So that` body — readers can't tell whether the feature
  serves a real need.
- **Acceptance criteria phrased as implementation steps.**
  "Implement the `--force` flag" is not testable; "`secret -f push`
  exits 0 against a fast-forwardable remote and prints
  `push -f origin ...`" is.
- **Filing a feature instead of a bug.** If the behaviour is
  broken, it's a bug — bugs jump ahead of features at the same
  priority. Do not file a feature to avoid the TDD requirement.
