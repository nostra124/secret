# `secret`

> GPG-encrypted secrets store, account-scoped, with pass(1)-style flat-file layout

## Install

    git clone https://github.com/nostra124/secret
    cd secret
    ./install --prefix=$HOME/.local

Or in two steps:

    ./configure --prefix=$HOME/.local
    make install

`secret` is a C program: `make` compiles the `libsecstore` library
(`lib/`) and links the `secret` binary (`src/`) against it. A C compiler
(`cc`/`gcc`) is required; the runtime still shells out to `gpg(1)`,
`git(1)`, `pass(1)`, `ssh(1)` and `qrencode(1)` for the heavy lifting, so
on-disk and on-wire formats stay compatible with the original shell tool
and its peers.

## Quick start

    secret help
    secret version

## Layout

| Path | Purpose |
|---|---|
| `bin/secret` | the compiled entry point (built by `make`) |
| `lib/` | `libsecstore` — the C library implementing every subcommand |
| `lib/secstore.h` | public library header |
| `src/secret.c` | the CLI dispatcher (flag parsing + shortcut + routing) |
| `docs/secret.md` | CLI contract reference |
| `share/man/man1/secret.1` | man page |
| `share/doc/secret/standards/` | vendored references (educational) |
| `skills/secret-user.md` | agent skill |
| `tests/unit/secret.bats` | unit tests |
| `tests/sit/` | system integration (when present) |
| `.cpk/` | container packaging overlay |
| `.rpk/` | rpk metadata (version, versions ledger, depends/) |

## Documentation

- `man secret`
- `docs/secret.md` — CLI contract reference
- `share/doc/secret/standards/README.md` — vendored standards
- `CLAUDE.md` — agent guide
- `skills/secret-user.md` — agent skill

## Conventions

This package follows the rpk per-script repo convention:

- Per-script repo: this repo contains only `secret`'s artefacts.
- No *sibling-tool* dependency: `secret` never calls `crypt` / `config` /
  `account` at runtime (see `CLAUDE.md` §4–5). `libsecstore` is `secret`'s
  own internal C library, shipped with this package — not a shared
  dependency on another tool.
- Stow-based install via `make install`.
- Versioning: semver, with `.rpk/version` as the source of truth and `.rpk/versions` as the per-release SHA ledger.

## License

GPL-3 (per the cross-cutting policy in the parent `scripts` collection).
