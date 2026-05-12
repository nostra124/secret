---
name: testing
description: |
  Run, extend, and debug the `secret` test suites. Trigger when the
  user asks how to run tests, why CI failed, where SIT lives, how
  pre-push works, or how to add a new test tier. Covers the three
  surfaces (unit / SIT / PIT), the environment-detecting pre-push
  hook, and the GitHub Actions workflow that posts failure logs back
  to the PR as comments.
---

# `testing` skill — three tiers, two environments

## 1. The three test tiers

| Tier | Path | Runner | Needs | Speed |
|---|---|---|---|---|
| **Unit** | `tests/unit/*.bats` | `bats` | bash, `bats`, no network, no real GPG | seconds |
| **SIT** (system integration) | `tests/sit/` | `tests/sit/run.sh` (per-distro podman matrix) | `podman`, real `gpg`, optional `pass` | minutes |
| **PIT** (per-image / packaging) | `tests/pit/` | `tests/pit/run.sh` | `podman` plus a built `.deb`/`.rpm`/stow tree | minutes |

Scope rules (also enforced by `tests/unit/secret.bats:1-16`):

- **Unit** covers every code path that does not need GPG, `pass`,
  `qrencode`, `sudo`, `ssh`, or the network. Argument validation,
  dispatcher routing, store-path math, output formatting.
- **SIT** covers the happy paths that need real binaries: `init`,
  `set`/`get` with real GPG, `pass-init`, `pull`/`push`/`sync`. Runs
  inside per-distro podman images so we exercise the missing-package
  guards in `command:pass-init` etc.
- **PIT** covers the packaging artefact itself: stow-installed
  layout, man page lookup, version-file resolution after `make
  install`, `./configure --prefix=...` permutations.

The unit suite is the only one that runs in CI today. SIT and PIT
are desktop-only (see §3).

## 2. Running tests locally

```sh
# Unit only (no podman needed)
make check-unit
# or directly:
bats tests/unit/*.bats

# SIT only — needs podman
make check-sit

# PIT only — needs podman
make check-pit

# Everything
make check-all
```

Each `check-*` target soft-skips with a clear message when its
prerequisite is missing, so `make check-all` is safe to run in any
environment.

## 3. The pre-push hook (`etc/hooks/pre-push`)

`etc/hooks/pre-push` is the versioned hook that runs before every
`git push`. Install it once per clone with:

```sh
make install-hooks    # symlinks etc/hooks/* into .git/hooks/
make uninstall-hooks  # removes the symlinks
```

Decision logic (mirrors `skills/testing.md` so they cannot drift):

```
1. SECRET_SKIP_TESTS=1?         → skip everything, exit 0
2. unit tests                   → always run (fail if bats missing)
3. podman available?
     yes  →  run SIT (if tests/sit/ exists) then PIT (if tests/pit/ exists)
     no   →  skip SIT and PIT, print a notice
```

Environment-specific behaviour, recorded so reviewers know what each
push exercised:

| Environment | Unit | SIT | PIT |
|---|---|---|---|
| Cloud sandbox (no podman) | ✓ | skipped | skipped |
| Desktop with podman | ✓ | ✓ (if present) | ✓ (if present) |
| CI (GitHub Actions) | ✓ | ✗ (intentional — SIT runs on desktop only) | ✗ |

**Skipping the hook**: `SECRET_SKIP_TESTS=1 git push` is reserved for
docs-only or roadmap-only pushes that don't touch `bin/secret`. Use
sparingly — the next CI run still has to pass.

## 4. GitHub Actions workflow (`.github/workflows/test.yml`)

- Triggers on every PR and on `push` to `master`.
- Installs `bats`, runs `bats tests/unit/*.bats`, captures the
  combined stdout+stderr to `test-output.log`.
- **On failure**, takes the last 200 lines of the log and posts it
  to the PR as a comment via `gh pr comment` (requires
  `permissions: pull-requests: write`). The comment includes a link
  back to the workflow run and the offending commit SHA.
- Always uploads the full log as a 7-day artefact.

Why post failures as PR comments:

- Lets reviewers (and the Claude assistant subscribed to the PR via
  `subscribe_pr_activity`) see the failure inline.
- Survives the workflow log being purged.
- Triggers a `pull_request_comment` webhook, which is what wakes the
  Claude session listening on the PR.

Failure messages are **trimmed to 200 lines** to stay safely under
GitHub's 65 535-character comment limit; the full log is available
as the `bats-unit-log` artefact.

## 5. Adding tests

### A new unit test

1. Add a `@test` block to `tests/unit/secret.bats` under the
   appropriate section header.
2. Keep it hermetic — no network, no real GPG. Use the existing
   `setup()` sandbox (`$XDG_SECRET_STORES`, sandboxed `$HOME`).
3. Run `bats tests/unit/secret.bats`; commit when green.

### A new SIT case

1. Create `tests/sit/<topic>.bats` (or `<topic>.sh`).
2. Wire it into `tests/sit/run.sh` so the per-distro matrix picks
   it up.
3. Add a podman fixture under `tests/sit/fixtures/` if needed.
4. Verify locally: `make check-sit`.

### A new PIT case

1. Create `tests/pit/<topic>.sh`.
2. Wire it into `tests/pit/run.sh`. Each case typically:
   - builds the package (`make install` into a throwaway prefix or
     `make package VERSION=...` into a tarball),
   - launches a clean podman container,
   - copies the artefact in,
   - asserts the installed layout and `secret version` output.
3. Verify locally: `make check-pit`.

## 6. Common failure patterns

- **`bats: command not found`** — install with
  `apt-get install bats` (Debian/Ubuntu) or
  `brew install bats-core` (macOS). On CI this is handled by the
  workflow.
- **A test sets `$HOME` but the binary still looks at the runner's
  home** — `setup()` exports `HOME=$(mktemp -d ...)`, but if a test
  reorders setup calls it can leak. Always export `HOME` *before*
  invoking `$SECRET_BIN`.
- **Hook installed but never runs** — check `ls -la .git/hooks/` for
  the symlink; remember that `git clone` does not preserve hooks, so
  every fresh clone needs `make install-hooks` again.
- **CI does not post a failure comment** — the workflow needs
  `pull-requests: write` permission. Forks running on the default
  `GITHUB_TOKEN` may receive a read-only token; the comment step
  exits cleanly in that case and the artefact is the fallback.

## 7. Where each tier's contract is recorded

| Tier | Contract location |
|---|---|
| Unit | `tests/unit/secret.bats` header + the `@test` titles |
| SIT | `tests/sit/README.md` (per FEAT-030) |
| PIT | `tests/pit/README.md` |
| CI | `.github/workflows/test.yml` (this file is the source of truth for what CI actually runs) |
| Pre-push | `etc/hooks/pre-push` (the script is the source of truth for what runs on push) |
