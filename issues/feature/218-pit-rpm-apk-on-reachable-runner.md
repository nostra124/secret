---
id: FEAT-218
type: feature
priority: medium
status: open
milestone: 0.15.0
---

# Run the rpm/apk PIT legs on a runner with reachable mirrors

## Description

**As a** release engineer
**I want** the rpm and apk package-integration tests to run to completion
in CI
**So that** all four native package formats (deb / rpm / apk / macOS pkg)
are verified end-to-end before a release, not just deb.

The PIT recipes for rpm and apk are complete and pass locally when the
distro mirrors are reachable. In the current sandbox they soft-skip
because the container registry / package mirrors are unreachable through
the self-signed TLS proxy; deb runs fully. This is an environment gap,
not a recipe gap.

## Implementation

1. Provide a CI runner (or a network policy) where Fedora/openSUSE and
   Alpine package mirrors are reachable, mounting the proxy CA bundle so
   `dnf`/`apk` TLS succeeds.
2. Flip the rpm/apk PIT legs from soft-skip to hard-fail when the mirror
   is reachable, keeping the soft-skip only for genuinely offline
   environments.
3. Wire `make check-pit` for all formats into the release checklist in
   `skills/version.md`.

## Acceptance Criteria

1. `make check-pit` installs and smoke-tests the rpm and apk packages on
   the CI runner.
2. A mirror-reachable environment produces a hard failure on a broken
   package, not a skip.
3. The release checklist requires a green four-format PIT run.
