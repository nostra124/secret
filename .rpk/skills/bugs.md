---
name: bugs
description: File and fix a bug in secret
long_description: |
  File and fix a bug in `secret`. Trigger when CI fails, a reviewer
  reports incorrect behaviour, or you discover a defect while
  working on something else. Covers the issue file format
  (`type: bug`), priority ordering (bugs precede features at the
  same level), and the mandatory test-driven workflow: a failing
  unit test reproducing the bug must land **before** the fix.
---

# `bugs` skill — TDD-first bug authoring and resolution

## 0. The rule

> **Every bug fix starts with a failing test. Write the test, see
> it fail, then write the fix.** A PR that fixes a bug without
> first introducing a reproducing test is incomplete.

This is TDD, not theatre. The test ensures:

- The defect is unambiguously characterised before any code moves.
- The fix is verified to actually resolve the bug (not adjacent
  behaviour).
- The defect cannot regress silently — the test stays in the suite.

When CI fails on `master`, the failure log goes to the PR as a
comment (see `skills/testing.md` §4). That comment is the input to
this workflow.

## 1. Bug priority sits above feature priority

Per `CLAUDE.md` §3: **bugs come before features at the same
priority level.** A `medium` bug is worked before a `medium`
feature; a `high` bug ahead of a `high` feature. The sequencing
section of every `issues/ROADMAP-<x.y.z>.md` reflects this — bugs
are always listed first within a sprint.

## 2. Issue file format

Bugs live under `issues/bug/<NNN>-<slug>.md` and use the same
`FEAT-NNN` numbering pool as features (CLAUDE.md §3). Frontmatter:

```yaml
---
id: FEAT-NNN
type: bug
priority: high | medium | low
status: open | in-progress | resolved
resolved-in: <x.y.z>        # added when the fix ships
---
```

Body sections (mandatory):

1. **Title** (`# <terse description of broken behaviour>`)
2. **Description** — As a / I want / So that, plus the surrounding
   context that makes the bug a bug.
3. **Steps to reproduce** — one or more shell commands that
   trigger the defect. Must be runnable as written.
4. **Root cause** — once known, the precise location (`bin/secret:L`)
   and the wrong line(s).
5. **Implementation** — what the fix does, plus the test that
   reproduces the bug. Reference the test by `@test "..."` title.
6. **Acceptance Criteria** — bulleted, includes "the reproducing
   test now passes" and any other observable effects.

Pattern examples in the tree: `issues/bug/200-*.md`,
`issues/bug/201-*.md`, `issues/bug/202-*.md`.

## 3. The TDD loop (mandatory)

```
1. Reproduce locally
   - Run the failing scenario by hand. Capture exit code and stderr.
   - If you cannot reproduce locally, the bug report is incomplete —
     close the loop with the reporter before continuing.

2. Write the failing test
   - Add a @test block under the appropriate section in
     tests/unit/secret.bats. Name it after the misbehaviour
     ("get on existing store with missing param emits error level").
   - Keep it hermetic; no network, no real GPG. Use the existing
     setup() sandbox.
   - Run: bats tests/unit/secret.bats — confirm the test FAILS.
     (Save the failure output for the commit body if you intend to
     push the test alone; otherwise keep going.)

3. Author the issue file
   - issues/bug/<NNN>-<slug>.md with type: bug, status: open.
   - Cross-reference the @test title under "Implementation".
   - Insert into the next ROADMAP-<x.y.z>.md sprint.

4. Implement the fix
   - Smallest change that makes the test pass. Do not refactor
     adjacent code in the same commit.
   - Re-run: bats tests/unit/secret.bats — confirm GREEN.

5. Commit and push
   - Single commit if test + fix are both small; otherwise two
     commits in order (test first, fix second) so `git log -p`
     reads as a TDD narrative.
   - Update the issue's frontmatter: status: resolved, resolved-in:
     <x.y.z>.

6. Land
   - CI runs the unit suite on the PR. On green, auto-merge fires
     (see skills/automerging.md). On red, the failure comment
     becomes a new bug — go to step 1 for that new bug.
```

## 4. CI failure → bug

When CI fails, the workflow posts the trimmed log as a PR comment.
Treat that comment as a bug report:

1. Open a new bug issue (`type: bug`, `priority: high` for any CI
   failure on `master`).
2. Step through the TDD loop above.
3. The reproducing test is whatever the CI runner saw; copy it
   verbatim from the comment if possible.

A CI failure that cannot be reproduced locally is its own bug
class — usually environment drift. Reproduce in a clean container:

```sh
podman run --rm -v $PWD:/work -w /work debian:stable bash -c \
  'apt-get update && apt-get install -y bats && bats tests/unit/*.bats'
```

(`skills/testing.md` §6 covers this in more depth.)

## 5. Common anti-patterns

- **Fixing first, "adding the test" after.** The whole point is
  that the test fails *before* the fix exists. Without that step,
  you cannot tell whether the test actually exercises the bug or
  merely the surrounding code.
- **Writing a test that always passes.** If the test stays green
  with the broken code, it does not exercise the bug. Confirm the
  failure manually before claiming TDD.
- **Bundling unrelated cleanups.** A bug fix should touch only the
  code path the failing test covers, plus the test itself. Save
  the cleanup for a separate feature issue.
- **Skipping the issue file.** Even a one-line typo fix gets an
  issue file. The roadmap is the project's memory; an unrecorded
  fix is one nobody can find six months later.
- **Lowering `priority`** to dodge sprint sequencing rules. If
  it's a bug, it sits above same-priority features. Do not
  reclassify to feature to move it later.

## 6. Cross-references

- `skills/testing.md` — how to run the test suites locally and in CI.
- `skills/features.md` — feature-issue authoring (the sibling
  process; bugs and features share the FEAT-NNN pool).
- `skills/automerging.md` — what happens after the fix is pushed.
- `skills/version.md` — when a bug fix triggers a version bump.
- `CLAUDE.md` §3, §8 — issue authoring rules and semver decision.
