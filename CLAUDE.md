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
- git-backed sync of stores between peers via SSH

Out of scope: cryptography primitives separate from GPG,
generic config file management.

`secret` is self-contained — it can be installed without
`account(1)`, but uses the same `~/.config/account/` config
directory layout for GPG public keys when `account(1)` is
installed.

## 2. Repo conventions

`secret` is a C program. The logic lives in `libsecstore`
(`lib/*.c`, public header `lib/secstore.h`); the CLI
(`src/secret.c`) is a thin dispatcher that parses the
global `-d/-q/-f` flags, applies the FEAT-205
`<store>/<param>` shortcut, and routes to one `cmd_<verb>`
function per subcommand (the C analogue of the old
`command:<verb>` shell functions). The single source of
truth for which verbs exist is the table in
`lib/dispatch.c`.

`make` compiles the library objects (`-fPIC`), links
`bin/secret` against them so the binary runs from the
source tree with no `LD_LIBRARY_PATH`, and also emits
`libsecstore.so` / `libsecstore.a`. `bin/secret` is a
build artifact (git-ignored), not a tracked script — the
unit suite, CI and the pre-push hook all build it first
(`make -f Makefile.in bin/secret`).

The runtime still shells out to `gpg(1)`, `git(1)`,
`pass(1)`, `ssh(1)` and `qrencode(1)` — the same external
tools the original shell version used — so on-disk
ciphertext and on-wire git formats remain interoperable
with peers running either implementation.

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

Planning happens through milestone backlog files
(`issues/ROADMAP-<x.y.z>.md`) — see `skills/milestones.md`
for the per-milestone session model, the shuffle protocol
when work moves between releases, the `issues/done/`
archive for resolved issues, and the retire commit that
removes a `ROADMAP` file once the release ships.
`skills/audit.md` describes the periodic traceability
check that keeps `issues/` honest: every behaviour the
software exhibits must map to at least one open or done
issue.

CI failures arrive as PR comments (see §7) — treat
each as a new bug per `skills/bugs.md`.

When merging, follow `skills/automerging.md`: every
PR that has been reviewed and whose tests pass should
auto-merge through the GitHub gate.

## 4. The no-shared-lib policy

`secret` is self-contained. It reads `~/.config/account/` config files
directly (GPG public keys, SSH public keys) for interoperability with
`account(1)`, but does not require it at runtime. All utility logic —
identity computation, GPG key management, account list enumeration —
is inlined.

The temptation to "DRY it up" by reaching for `crypt` /
`config` / `data` / `user` / `account` is **wrong**. These are
sibling tools, not libraries. `secret` operates standalone.

This policy is about *sibling tools*, not internal code
organisation. `libsecstore` (`lib/`) **is** `secret`'s
own library, shipped inside this package and built from
this repo — splitting the implementation into a library
plus a CLI does not violate the policy. The named
sibling tools above must still never be linked or called.
(`libsecstore` is deliberately *not* called `libsecret`,
to avoid the collision with the unrelated GNOME /
freedesktop Secret Service library of that name.)

## 5. What is intentionally duplicated

- **Identity computation.** `$(whoami)@$(hostname -f)` — same format
  as `account identity`, computed directly without calling `account`.
- **GPG key management.** `gpg_export_public_key`, `gpg_import_public_key`,
  `gpg_fingerprint`, `gpg_encrypt`, `gpg_decrypt` — direct GPG calls,
  storing exported keys in `~/.config/account/gpg/<identity>.pub` for
  interoperability with `account(1)`.
- **Account list.** Reads `~/.config/account/ssh/` directory listing,
  removing `.pub` suffix — same data source as `account list`.
- **GPG encrypt / decrypt invocation.** Direct calls to `gpg(1)`;
  never delegated to `crypt` or `account`.
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
FEAT-191). `make` compiles `libsecstore` and the `secret`
binary (needs a C compiler); `make install` stows the
binary, the library (`libsecstore.so` / `.a`), the public
header (`secstore.h`), the man page and completion.
`master` is always installable; releases are tagged via
`.rpk/version`.

Native packages live under `packaging/` (deb / rpm / apk /
macOS pkg). Each `packaging/<fmt>/build.sh` stages files
via the `install-dist` Makefile target (a plain, non-stow
`DESTDIR` + `prefix` install) and wraps them with the
platform's packager; `make package-<fmt>` or `make
packages` drive them. Output goes to `dist/` (git-ignored).
See `packaging/README.md`.

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
