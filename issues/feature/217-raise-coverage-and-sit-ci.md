---
id: FEAT-217
type: feature
priority: high
status: open
milestone: 0.15.0
---

# Raise coverage to ~95%+ and run the SIT suite in CI with a gate

## Description

**As a** maintainer
**I want** the SIT suite to run in CI (with the real tools installed) and
a coverage floor enforced on every PR
**So that** regressions in the shell-out paths (gpg / git / ssh / http)
are caught automatically rather than only under manual SIT runs.

PR #14 landed the unit/SIT/PIT layering and fault injection, reaching
~89% line coverage. The remaining gap is dominated by shell-out paths
that only execute when the real tools are present. Those are exercised by
the SIT suite, which currently runs only locally.

## Implementation

1. Add a CI job that installs `gpg`, `git`, `openssh`, `qrencode`,
   `oathtool` and runs `make check-sit` with `SECRET_SIT=1`.
2. Compute coverage in that job (`make coverage`) and fail the build if
   the total line rate is below `SECRET_COV_MIN` (start at 95).
3. Close the residual fault-injection gaps identified in
   `.rpk/skills/testing.md` (EINTR/partial-write loops, clip backends,
   getpwuid/`/proc` fallbacks) where a fault point can reach them.
4. Document the enforced floor and how to reproduce the CI coverage run
   locally in `skills/testing.md`.

## Acceptance Criteria

1. CI runs the SIT suite with the real tools on every PR.
2. A `SECRET_COV_MIN` gate fails the build below the floor (≥95).
3. Line coverage is ≥95% with the SIT + fault legs combined.
4. `skills/testing.md` documents the floor and the local reproduction.
