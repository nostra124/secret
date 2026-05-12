# `secret` — developer notes

> The `CLAUDE.md` shipped with the extracted
> `nostra124/secret` repo. Mirrors the eight-section
> structure in `docs/templates/CLAUDE.md.foundation`,
> specialised for `secret`.

## 1. Scope

`secret` is the multi-store secret manager. Its scope is:

- managing GPG-encrypted-at-rest stores (one git repo per
  store) under `$XDG_CONFIG_HOME/secret/<store>/`
- the `pass(1)` password-store integration
  (`pass-init` per FEAT-041)
- git-backed sync of stores between
  `account`-registered peers via `account remote-url`
  (FEAT-044)

Out of scope: cryptography primitives (that's `crypt`),
identity/key management (that's `account`), generic
config file management (that's `config`).

## 2. Repo conventions

Standard rpk per-package conventions: `bin/secret`
dispatcher with `command:<verb>` functions. No plugins
in `libexec/secret/` today; if a future plugin lands,
it follows FEAT-001's libexec lookup pattern.

Logging: four levels (`debug` / `info` / `warn` /
`error`) plus a `fatal` convenience (= error + exit).
See `skills/logging.md` for when to use each, the
env-var switches (`SELF_DEBUG`, `SELF_QUIET`), and the
output-format contract tests pin against.

## 3. Issue authoring

Same as `CLAUDE.md.foundation`. Frontmatter with
`id: FEAT-NNN`. **Bugs come before features at the same
priority level.**

The mechanics are split across two skills so each has
its own discoverable trigger:

- `skills/features.md` — how to author a feature issue
  (`type: feature`), user story format, acceptance
  criteria.
- `skills/bugs.md` — how to author a bug issue
  (`type: bug`) and the **mandatory TDD workflow**: a
  failing unit test that reproduces the bug lands
  before the fix.

CI failures arrive as PR comments (see §7) — treat
each as a new bug per `skills/bugs.md`.

When merging, follow `skills/automerging.md`: every
PR that has been reviewed and whose tests pass should
auto-merge through the GitHub gate.

## 4. The no-shared-lib policy

`secret` calls only `account` at runtime. Every other
utility logic — store-path computation, conflict
resolution, parameter encoding — is inlined.

The temptation to "DRY it up" by reaching for `crypt` /
`config` / `data` / `user` is **wrong**. `crypt` is a
crypto-primitives sibling, not a library. `config` is a
sibling tool, not a config-reader.

## 5. What is intentionally duplicated

- **GPG encrypt / decrypt invocation.** Direct calls to
  `gpg(1)` (via `account gpg-encrypt` / `account
  gpg-decrypt`); never delegated to `crypt`.
- **Store-path computation** (`$XDG_CONFIG_HOME/secret/`
  fallback to `/etc/secret/` when root). Same shape as
  `config`'s store-path logic, intentionally duplicated.
- **Conflict resolution on git pull** (`git checkout
  --theirs $(git diff --name-only --diff-filter=U)`).
  Direct calls to `git(1)`.

## 6. Consumers

`bsync`, `container`, `crypt`, `hosts`, `bitcoin`
(post-FEAT-010), and any other script that needs
encrypted-at-rest storage of structured secrets.

## 7. Build / install

`./configure && make install` (autoconf umbrella per
FEAT-191). Stow-based install. `master` is always
installable; releases are tagged via `.rpk/version`.

After cloning, install the versioned git hooks with
`make install-hooks` so the pre-push test gate runs
locally. See `skills/testing.md` for the full
unit/SIT/PIT layering, the CI workflow that posts
failure logs back to the PR as comments, and the
environment-detection logic in `etc/hooks/pre-push`.

## 8. Versioning

Semver. The canonical version lives in `VERSION` at
the project root; `.rpk/version` is kept as a FEAT-194
mirror. Every release is recorded in `.rpk/versions`
(TSV ledger \(em no orphan SHAs per rpk's BUG-001
lesson). Any change that breaks the bats contract in
`tests/unit/secret.bats` requires a major version
bump.

See `skills/version.md` for the full release flow:
where each version file lives, the semver decision
rules, and the manual / `make package` release steps.
