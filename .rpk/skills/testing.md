---
name: testing
description: |
  Run, extend, and debug the `secret` test suites. Trigger when the
  user asks how to run tests, why CI failed, where SIT lives, how
  pre-push works, or how to add a new test tier. Covers the three
  surfaces (unit / SIT / PIT), the environment-detecting pre-push
  hook, and the GitHub Actions workflow that gates merging by
  posting failure logs back to the PR as comments.
---

# `testing` skill — local first, CI mandatory

## 0. The workflow rule

> **Local tests are the primary signal. CI is the merge gate. Every
> change must satisfy both, in that order.**

Day-to-day development is driven by local runs. CI exists to catch
the long tail (environment drift, missing-dependency guards) and to
gate merging — it is **not** where you discover whether your change
works.

The order of every change:

```
1. Edit code.
2. Run unit tests locally          → must be green.
3. Run SIT/PIT locally (if podman) → must be green.
4. Commit, push.
5. CI runs the unit suite          → MUST be green before merge.
6. Reviewer approves               → merge.
```

Steps 2–3 are enforced by `etc/hooks/pre-push` (see §3). Step 5 is
the **mandatory** GitHub Actions check — see §4. A red CI is a
blocking failure regardless of any local-only signal.

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

## 2. Running tests locally (start here)

Local is the **first signal** for every change. If your change
touches `bin/secret`, you must run the unit suite locally before
committing — the pre-push hook will enforce it, but you should
not rely on the hook as your first check.

```sh
# Unit only — fastest, no podman needed. Run this every save.
make check-unit
# or directly:
bats tests/unit/*.bats

# SIT — needs podman. Run when touching anything that uses gpg/pass.
make check-sit

# PIT — needs podman. Run when touching configure / Makefile /
# share/secret/version resolution.
make check-pit

# Everything (soft-skips tiers whose prerequisites are missing).
make check-all
```

Each `check-*` target soft-skips with a clear message when its
prerequisite is missing, so `make check-all` is safe to run in any
environment.

**Rule of thumb**: a change that crosses a tier boundary requires
running that tier locally. Adding a subcommand → unit. Touching
GPG invocation → unit + SIT. Touching `configure` or `make install`
→ unit + PIT.

## 3. The pre-push hook (`etc/hooks/pre-push`) — local gate

`etc/hooks/pre-push` is the versioned hook that enforces local
tests before every `git push`. Install it once per clone:

```sh
make install-hooks    # symlinks etc/hooks/* into .git/hooks/
make uninstall-hooks  # removes the symlinks
```

`make install-hooks` is part of the standard post-clone checklist
alongside `./configure`. Do it on every fresh clone.

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

**Bypassing the hook is reserved for true exceptions**:
`SECRET_SKIP_TESTS=1 git push` for docs-only or roadmap-only pushes
that don't touch any tested file. Use sparingly — the next CI run
still has to pass, and reviewers will look skeptically at a bypass
on a code change.

## 4. CI is mandatory (`.github/workflows/test.yml`)

> A green CI run is **required** to merge into `master`. A red CI
> blocks the merge even if local tests passed; investigate and fix.

- Triggers on every PR and on `push` to `master`.
- Installs `bats`, runs `bats tests/unit/*.bats`, captures the
  combined stdout+stderr to `test-output.log`.
- **On failure**, takes the last 200 lines of the log and posts it
  to the PR as a comment via `gh pr comment` (requires
  `permissions: pull-requests: write`). The comment includes a link
  back to the workflow run and the offending commit SHA.
- Always uploads the full log as a 7-day artefact.

The PR-comment behaviour is intentional and load-bearing:

- Reviewers see the failure inline without leaving the PR.
- It survives the workflow log being purged.
- It triggers a `pull_request_comment` webhook, which wakes any
  Claude session subscribed to the PR via `subscribe_pr_activity`.

CI also emits **`check_run` events** when a job finishes —
delivered through the same webhook subscription. The agent reacts
differently to each conclusion (see `skills/automerging.md` §2b):

| `check_run.conclusion` | Agent action |
|---|---|
| `success` | Re-check merge prerequisites; auto-merge or manual-merge per `skills/automerging.md` |
| `failure` | Open a bug via `skills/bugs.md`; do **not** merge |
| `cancelled` / `timed_out` / `action_required` | Treat as failure; investigate first, re-trigger only if confirmed flake |

The agent **must subscribe** via `subscribe_pr_activity` and react
to events. Polling `get_check_runs` in a loop, sleeping until CI
finishes, or any other busy-wait pattern is forbidden — it hangs
the session and produces no work.

Webhook delivery is best-effort. To handle dropped events, the
agent also reconciles state at the start of every wake — one
`get_check_runs` call per subscribed PR per turn. See
`skills/automerging.md` §2c. CI failures (and successes) that
miss the webhook still surface on the next turn.

Failure messages are **trimmed to 200 lines** to stay safely under
GitHub's 65 535-character comment limit; the full log is available
as the `bats-unit-log` artefact.

### Enforcing the mandate at the repo level

Branch protection on `master` should require the `unit` check
before merging. Configure under `Settings → Branches → Branch
protection rules`:

- ✅ Require status checks to pass before merging
- ✅ Require branches to be up to date before merging
- Selected checks: `unit`

This makes the "CI mandatory" rule machine-enforced rather than
relying on reviewer discipline. Until that is configured, the
mandate is by convention — reviewers must check the CI status
themselves.

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
- **"It worked locally but CI failed"** — almost always an
  environment leak (uninstalled dependency, ambient `$XDG_*` var,
  cached `.password-store`). Reproduce in a clean container:
  `podman run --rm -v $PWD:/work -w /work debian:stable bash -c \
   'apt-get update && apt-get install -y bats && bats tests/unit/*.bats'`.

## 7. Where each tier's contract is recorded

| Tier | Contract location |
|---|---|
| Unit | `tests/unit/secret.bats` header + the `@test` titles |
| SIT | `tests/sit/README.md` (per FEAT-030) |
| PIT | `tests/pit/README.md` |
| CI | `.github/workflows/test.yml` (this file is the source of truth for what CI actually runs) |
| Pre-push | `etc/hooks/pre-push` (the script is the source of truth for what runs on push) |
